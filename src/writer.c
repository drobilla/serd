// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_sink.h"
#include "env.h"
#include "node.h"
#include "serd_internal.h"
#include "sink.h"
#include "stack.h"
#include "string_utils.h"
#include "try.h"
#include "uri_utils.h"
#include "warnings.h"

#include "serd/attributes.h"
#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/error.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/string_view.h"
#include "serd/syntax.h"
#include "serd/uri.h"
#include "serd/writer.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  CTX_NAMED, ///< Normal non-anonymous context
  CTX_BLANK, ///< Anonymous blank node
  CTX_LIST,  ///< Anonymous list
} ContextType;

typedef enum {
  FIELD_NONE,
  FIELD_SUBJECT,
  FIELD_PREDICATE,
  FIELD_OBJECT,
  FIELD_GRAPH,
} Field;

typedef struct {
  ContextType type;
  SerdNode*   graph;
  SerdNode*   subject;
  SerdNode*   predicate;
  bool        predicates;
  bool        comma_indented;
} WriteContext;

static const WriteContext WRITE_CONTEXT_NULL =
  {CTX_NAMED, NULL, NULL, NULL, 0U, 0U};

typedef enum {
  SEP_NONE,        ///< Sentinel before the start of a document
  SEP_NODE,        ///< Sentinel after a node
  SEP_NEWLINE,     ///< Sentinel after a node
  SEP_END_DIRECT,  ///< End of a directive (like "@prefix")
  SEP_END_S,       ///< End of a subject ('.')
  SEP_END_P,       ///< End of a predicate (';')
  SEP_END_O,       ///< End of a named object (',')
  SEP_JOIN_O_AN,   ///< End of anonymous object (',') before a named one
  SEP_JOIN_O_NA,   ///< End of named object (',') before an anonymous one
  SEP_JOIN_O_AA,   ///< End of anonymous object (',') before another
  SEP_S_P,         ///< Between a subject and predicate (whitespace)
  SEP_P_O,         ///< Between a predicate and object (whitespace)
  SEP_ANON_BEGIN,  ///< Start of anonymous node ('[')
  SEP_ANON_S_P,    ///< Between anonymous subject and predicate (whitespace)
  SEP_ANON_END,    ///< End of anonymous node (']')
  SEP_LIST_BEGIN,  ///< Start of list ('(')
  SEP_LIST_SEP,    ///< List separator (whitespace)
  SEP_LIST_END,    ///< End of list (')')
  SEP_GRAPH_BEGIN, ///< Start of graph ('{')
  SEP_GRAPH_END,   ///< End of graph ('}')
} Sep;

typedef uint32_t SepMask; ///< Bitfield of separator flags

typedef struct {
  char    sep;             ///< Sep character
  int     indent;          ///< Indent delta
  SepMask pre_space_after; ///< Leading space if after given seps
  SepMask pre_line_after;  ///< Leading newline if after given seps
  SepMask post_line_after; ///< Trailing newline if after given seps
} SepRule;

#define SEP_EACH (~(SepMask)0)
#define M(s) (1U << (s))
#define NIL '\0'

static const SepRule rules[] = {
  {NIL, +0, SEP_NONE, SEP_NONE, SEP_NONE},
  {NIL, +0, SEP_NONE, SEP_NONE, SEP_NONE},
  {'\n', 0, SEP_NONE, SEP_NONE, SEP_NONE},
  {'.', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {'.', +0, SEP_EACH, SEP_NONE, SEP_NONE},
  {';', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_NONE},
  {NIL, +1, SEP_NONE, SEP_NONE, SEP_EACH},
  {' ', +0, SEP_NONE, SEP_NONE, SEP_NONE},
  {'[', +1, M(SEP_JOIN_O_AA), SEP_NONE, SEP_NONE},
  {NIL, +1, SEP_NONE, SEP_NONE, M(SEP_ANON_BEGIN)},
  {']', -1, SEP_NONE, ~M(SEP_ANON_BEGIN), SEP_NONE},
  {'(', +1, M(SEP_JOIN_O_AA), SEP_NONE, SEP_EACH},
  {NIL, +0, SEP_NONE, SEP_EACH, SEP_NONE},
  {')', -1, SEP_NONE, SEP_EACH, SEP_NONE},
  {'{', +1, SEP_EACH, SEP_NONE, SEP_EACH},
  {'}', -1, SEP_NONE, SEP_NONE, SEP_EACH},
};

#undef NIL
#undef M
#undef SEP_EACH

struct SerdWriterImpl {
  SerdSink        iface;
  SerdSyntax      syntax;
  SerdWriterFlags flags;
  SerdEnv*        env;
  SerdNode*       root_node;
  SerdURIView     root_uri;
  SerdStack       anon_stack;
  SerdByteSink    byte_sink;
  SerdErrorFunc   error_func;
  void*           error_handle;
  WriteContext    context;
  char*           bprefix;
  size_t          bprefix_len;
  Sep             last_sep;
  int             indent;
};

typedef enum { WRITE_STRING, WRITE_LONG_STRING } TextContext;
typedef enum { RESET_GRAPH = 1U << 0U, RESET_INDENT = 1U << 1U } ResetFlag;

SERD_NODISCARD static SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri);

SERD_NODISCARD static SerdStatus
write_node(SerdWriter*        writer,
           const SerdNode*    node,
           Field              field,
           SerdStatementFlags flags);

SERD_NODISCARD static bool
supports_abbrev(const SerdWriter* writer)
{
  return writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG;
}

SERD_NODISCARD static bool
supports_uriref(const SerdWriter* writer)
{
  return writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG;
}

static SerdStatus
free_context(WriteContext* const ctx)
{
  serd_node_free(ctx->graph);
  serd_node_free(ctx->subject);
  serd_node_free(ctx->predicate);
  ctx->graph     = NULL;
  ctx->subject   = NULL;
  ctx->predicate = NULL;
  return SERD_SUCCESS;
}

SERD_LOG_FUNC(3, 4)
static SerdStatus
w_err(SerdWriter* writer, SerdStatus st, const char* fmt, ...)
{
  /* TODO: This results in errors with no file information, which is not
     helpful when re-serializing a file (particularly for "undefined
     namespace prefix" errors.  The statement sink API needs to be changed to
     add a Cursor parameter so the source can notify the writer of the
     statement origin for better error reporting. */

  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);
  const SerdError e = {st, "", 0, 0, fmt, &args};
  serd_error(writer->error_func, writer->error_handle, &e);
  va_end(args);
  return st;
}

static inline SerdNode*
ctx(SerdWriter* writer, const Field field)
{
  SerdNode* node = (field == FIELD_SUBJECT)     ? writer->context.subject
                   : (field == FIELD_PREDICATE) ? writer->context.predicate
                   : (field == FIELD_GRAPH)     ? writer->context.graph
                                                : NULL;

  return node && node->type ? node : NULL;
}

SERD_NODISCARD static SerdStatus
push_context(SerdWriter* const     writer,
             const ContextType     type,
             const SerdNode* const graph,
             const SerdNode* const subject,
             const SerdNode* const predicate)
{
  // Push the current context to the stack
  void* const top = serd_stack_push(&writer->anon_stack, sizeof(WriteContext));
  if (!top) {
    return SERD_BAD_STACK;
  }

  *(WriteContext*)top = writer->context;

  // Update the current context

  const WriteContext current = {type,
                                serd_node_copy(graph),
                                serd_node_copy(subject),
                                serd_node_copy(predicate),
                                0U,
                                0U};

  writer->context = current;
  return SERD_SUCCESS;
}

static void
pop_context(SerdWriter* writer)
{
  // Replace the current context with the top of the stack
  free_context(&writer->context);
  writer->context =
    *(WriteContext*)(writer->anon_stack.buf + writer->anon_stack.size -
                     sizeof(WriteContext));

  // Pop the top of the stack away
  serd_stack_pop(&writer->anon_stack, sizeof(WriteContext));
}

SERD_NODISCARD static size_t
sink(const void* buf, size_t len, SerdWriter* writer)
{
  const size_t written = serd_byte_sink_write(buf, len, &writer->byte_sink);
  if (written != len) {
    if (errno) {
      const char* const message = strerror(errno);
      w_err(writer, SERD_BAD_WRITE, "write error (%s)\n", message);
    } else {
      w_err(writer, SERD_BAD_WRITE, "write error\n");
    }
  }

  return written;
}

SERD_NODISCARD static inline SerdStatus
esink(const void* buf, size_t len, SerdWriter* writer)
{
  return sink(buf, len, writer) == len ? SERD_SUCCESS : SERD_BAD_WRITE;
}

// Write a single character, as an escape for single byte characters
// (Caller prints any single byte characters that don't need escaping)
static size_t
write_character(SerdWriter*    writer,
                const uint8_t* utf8,
                size_t*        size,
                SerdStatus*    st)
{
  char           escape[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const uint32_t c          = parse_utf8_char(utf8, size);
  switch (*size) {
  case 0:
    *st = w_err(writer, SERD_BAD_TEXT, "invalid UTF-8 start: %X\n", utf8[0]);
    return 0;
  case 1:
    snprintf(escape, sizeof(escape), "\\u%04X", utf8[0]);
    return sink(escape, 6, writer);
  default:
    break;
  }

  if (!(writer->flags & SERD_WRITE_ASCII)) {
    // Write UTF-8 character directly to UTF-8 output
    return sink(utf8, *size, writer);
  }

  if (c <= 0xFFFF) {
    snprintf(escape, sizeof(escape), "\\u%04X", c);
    return sink(escape, 6, writer);
  }

  snprintf(escape, sizeof(escape), "\\U%08X", c);
  return sink(escape, 10, writer);
}

static bool
uri_must_escape(const int c)
{
  switch (c) {
  case ' ':
  case '"':
  case '<':
  case '>':
  case '\\':
  case '^':
  case '`':
  case '{':
  case '|':
  case '}':
    return true;
  default:
    return !in_range(c, 0x20, 0x7E);
  }
}

static size_t
write_uri(SerdWriter* writer, const char* utf8, size_t n_bytes, SerdStatus* st)
{
  size_t len = 0;
  for (size_t i = 0; i < n_bytes;) {
    size_t j = i; // Index of next character that must be escaped
    for (; j < n_bytes; ++j) {
      if (uri_must_escape(utf8[j])) {
        break;
      }
    }

    // Bulk write all characters up to this special one
    const size_t n_bulk = sink(&utf8[i], j - i, writer);
    len += n_bulk;
    if (n_bulk != j - i) {
      *st = SERD_BAD_WRITE;
      return len;
    }

    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    // Write UTF-8 character
    size_t size = 0;
    len += write_character(writer, (const uint8_t*)utf8 + i, &size, st);
    i += size;
    if (*st && (writer->flags & SERD_WRITE_STRICT)) {
      break;
    }

    if (size == 0) {
      // Corrupt input, write percent-encoded bytes and scan to next start
      char escape[4] = {0, 0, 0, 0};
      for (; i < n_bytes && (utf8[i] & 0x80); ++i) {
        snprintf(escape, sizeof(escape), "%%%02X", (uint8_t)utf8[i]);
        len += sink(escape, 3, writer);
      }
    }
  }

  return len;
}

SERD_NODISCARD static SerdStatus
ewrite_uri(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
  SerdStatus st = SERD_SUCCESS;
  write_uri(writer, utf8, n_bytes, &st);

  return (st == SERD_BAD_WRITE || (writer->flags & SERD_WRITE_STRICT))
           ? st
           : SERD_SUCCESS;
}

SERD_NODISCARD static SerdStatus
write_uri_from_node(SerdWriter* writer, const SerdNode* node)
{
  return ewrite_uri(writer, serd_node_string(node), serd_node_length(node));
}

static bool
lname_must_escape(const char c)
{
  /* This arbitrary list of characters, most of which have nothing to do with
     Turtle, must be handled as special cases here because the RDF and SPARQL
     WGs are apparently intent on making the once elegant Turtle a baroque
     and inconsistent mess, throwing elegance and extensibility completely
     out the window for no good reason.

     Note '-', '.', and '_' are also in PN_LOCAL_ESC, but are valid unescaped
     in local names, so they are not escaped here. */

  switch (c) {
  case '\'':
  case '!':
  case '#':
  case '$':
  case '%':
  case '&':
  case '(':
  case ')':
  case '*':
  case '+':
  case ',':
  case '/':
  case ';':
  case '=':
  case '?':
  case '@':
  case '~':
    return true;
  default:
    break;
  }
  return false;
}

SERD_NODISCARD static SerdStatus
write_lname(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
  SerdStatus st = SERD_SUCCESS;
  for (size_t i = 0; i < n_bytes; ++i) {
    size_t j = i; // Index of next character that must be escaped
    for (; j < n_bytes; ++j) {
      if (lname_must_escape(utf8[j])) {
        break;
      }
    }

    // Bulk write all characters up to this special one
    TRY(st, esink(&utf8[i], j - i, writer));
    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    // Write escape
    TRY(st, esink("\\", 1, writer));
    TRY(st, esink(&utf8[i], 1, writer));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_text(SerdWriter* writer,
           TextContext ctx,
           const char* utf8,
           size_t      n_bytes)
{
  size_t     n_consecutive_quotes = 0;
  SerdStatus st                   = SERD_SUCCESS;
  for (size_t i = 0; !st && i < n_bytes;) {
    if (utf8[i] != '"') {
      n_consecutive_quotes = 0;
    }

    // Fast bulk write for long strings of printable ASCII
    size_t j = i;
    for (; j < n_bytes; ++j) {
      if (utf8[j] == '\\' || utf8[j] == '"' ||
          (!in_range(utf8[j], 0x20, 0x7E))) {
        break;
      }
    }

    st = esink(&utf8[i], j - i, writer);
    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    const char in = utf8[i++];
    if (ctx == WRITE_LONG_STRING) {
      n_consecutive_quotes = (in == '\"') ? (n_consecutive_quotes + 1) : 0;

      switch (in) {
      case '\\':
        st = esink("\\\\", 2, writer);
        continue;
      case '\b':
        st = esink("\\b", 2, writer);
        continue;
      case '\n':
      case '\r':
      case '\t':
      case '\f':
        st = esink(&in, 1, writer); // Write character as-is
        continue;
      case '\"':
        if (n_consecutive_quotes >= 3 || i == n_bytes) {
          // Two quotes in a row, or quote at string end, escape
          st = esink("\\\"", 2, writer);
        } else {
          st = esink(&in, 1, writer);
        }
        continue;
      default:
        break;
      }
    } else {
      switch (in) {
      case '\\':
        st = esink("\\\\", 2, writer);
        continue;
      case '\n':
        st = esink("\\n", 2, writer);
        continue;
      case '\r':
        st = esink("\\r", 2, writer);
        continue;
      case '\t':
        st = esink("\\t", 2, writer);
        continue;
      case '"':
        st = esink("\\\"", 2, writer);
        continue;
      default:
        break;
      }
      if (writer->syntax == SERD_TURTLE) {
        switch (in) {
        case '\b':
          st = esink("\\b", 2, writer);
          continue;
        case '\f':
          st = esink("\\f", 2, writer);
          continue;
        default:
          break;
        }
      }
    }

    // Write UTF-8 character
    size_t size = 0;
    write_character(writer, (const uint8_t*)utf8 + i - 1, &size, &st);
    if (st && (writer->flags & SERD_WRITE_STRICT)) {
      return st;
    }

    if (size == 0) {
      // Corrupt input, write replacement character and scan to the next start
      st = esink(replacement_char, sizeof(replacement_char), writer);
      for (; i < n_bytes && (utf8[i] & 0x80); ++i) {
      }
    } else {
      i += size - 1;
    }
  }

  return (writer->flags & SERD_WRITE_STRICT) ? st : SERD_SUCCESS;
}

typedef struct {
  SerdWriter* writer;
  SerdStatus  status;
} UriSinkContext;

SERD_NODISCARD static size_t
uri_sink(const void* buf, size_t size, size_t nmemb, void* stream)
{
  (void)size;
  assert(size == 1);

  UriSinkContext* const context = (UriSinkContext*)stream;
  SerdWriter* const     writer  = context->writer;

  return write_uri(writer, (const char*)buf, nmemb, &context->status);
}

SERD_NODISCARD static SerdStatus
write_newline(SerdWriter* writer)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, esink("\n", 1, writer));
  for (int i = 0; i < writer->indent; ++i) {
    TRY(st, esink("\t", 1, writer));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_sep(SerdWriter* writer, const Sep sep)
{
  SerdStatus           st   = SERD_SUCCESS;
  const SepRule* const rule = &rules[sep];

  const bool pre_line  = (rule->pre_line_after & (1U << writer->last_sep));
  const bool post_line = (rule->post_line_after & (1U << writer->last_sep));

  // Adjust indent, but tolerate if it would become negative
  if (rule->indent && (pre_line || post_line)) {
    writer->indent = ((rule->indent >= 0 || writer->indent >= -rule->indent)
                        ? writer->indent + rule->indent
                        : 0);
  }

  // If this is the first comma, bump the increment for the following object
  if (sep == SEP_END_O && !writer->context.comma_indented) {
    ++writer->indent;
    writer->context.comma_indented = true;
  }

  // Write newline or space before separator if necessary
  if (pre_line) {
    TRY(st, write_newline(writer));
  } else if (rule->pre_space_after & (1U << writer->last_sep)) {
    TRY(st, esink(" ", 1, writer));
  }

  // Write actual separator string
  if (rule->sep) {
    TRY(st, esink(&rule->sep, 1, writer));
  }

  // Write newline after separator if necessary
  if (post_line) {
    TRY(st, write_newline(writer));
    if (rule->post_line_after != ~(SepMask)0U) {
      writer->last_sep = SEP_NEWLINE;
    }
  }

  // Reset context and write a blank line after ends of subjects
  if (sep == SEP_END_S) {
    writer->indent                 = ctx(writer, FIELD_GRAPH) ? 1 : 0;
    writer->context.predicates     = false;
    writer->context.comma_indented = false;
    TRY(st, esink("\n", 1, writer));
  }

  writer->last_sep = sep;
  return st;
}

static void
free_anon_stack(SerdWriter* writer)
{
  while (!serd_stack_is_empty(&writer->anon_stack)) {
    pop_context(writer);
  }
}

static SerdStatus
reset_context(SerdWriter* writer, const unsigned flags)
{
  free_anon_stack(writer);

  if (writer->context.predicate) {
    memset(writer->context.predicate, 0, sizeof(SerdNode));
  }

  if (writer->context.subject) {
    memset(writer->context.subject, 0, sizeof(SerdNode));
  }

  if (flags & RESET_GRAPH) {
    if (writer->context.graph) {
      memset(writer->context.graph, 0, sizeof(SerdNode));
    }
  }

  if (flags & RESET_INDENT) {
    writer->indent = 0;
  }

  writer->context.type           = CTX_NAMED;
  writer->context.predicates     = false;
  writer->context.comma_indented = false;
  return SERD_SUCCESS;
}

SERD_NODISCARD static SerdStatus
write_literal(SerdWriter* const        writer,
              const SerdNode* const    node,
              const SerdStatementFlags flags)
{
  SerdStatus            st       = SERD_SUCCESS;
  const SerdNode* const datatype = serd_node_datatype(node);
  const SerdNode* const lang     = serd_node_language(node);
  const char* const     node_str = serd_node_string(node);
  const char* const     type_uri = datatype ? serd_node_string(datatype) : NULL;

  if (supports_abbrev(writer) && type_uri) {
    if (!strncmp(type_uri, NS_XSD, sizeof(NS_XSD) - 1) &&
        (!strcmp(type_uri + sizeof(NS_XSD) - 1, "boolean") ||
         !strcmp(type_uri + sizeof(NS_XSD) - 1, "integer"))) {
      return esink(node_str, node->length, writer);
    }

    if (!strncmp(type_uri, NS_XSD, sizeof(NS_XSD) - 1) &&
        !strcmp(type_uri + sizeof(NS_XSD) - 1, "decimal") &&
        strchr(node_str, '.') && node_str[node->length - 1] != '.') {
      /* xsd:decimal literals without trailing digits, e.g. "5.", can
         not be written bare in Turtle.  We could add a 0 which is
         prettier, but changes the text and breaks round tripping.
      */
      return esink(node_str, node->length, writer);
    }
  }

  if (supports_abbrev(writer) &&
      (node->flags & (SERD_HAS_NEWLINE | SERD_HAS_QUOTE))) {
    TRY(st, esink("\"\"\"", 3, writer));
    TRY(st, write_text(writer, WRITE_LONG_STRING, node_str, node->length));
    TRY(st, esink("\"\"\"", 3, writer));
  } else {
    TRY(st, esink("\"", 1, writer));
    TRY(st, write_text(writer, WRITE_STRING, node_str, node->length));
    TRY(st, esink("\"", 1, writer));
  }
  if (lang && serd_node_string(lang)) {
    TRY(st, esink("@", 1, writer));
    TRY(st, esink(serd_node_string(lang), lang->length, writer));
  } else if (type_uri) {
    TRY(st, esink("^^", 2, writer));
    return write_node(writer, datatype, FIELD_NONE, flags);
  }

  return st;
}

// Return true iff `buf` is a valid prefixed name prefix or suffix
static bool
is_name(const char* buf, const size_t len)
{
  // TODO: This is more strict than it should be
  for (size_t i = 0; i < len; ++i) {
    if (!(is_alpha(buf[i]) || is_digit(buf[i]))) {
      return false;
    }
  }

  return true;
}

SERD_NODISCARD static SerdStatus
write_uri_node(SerdWriter* const writer,
               const SerdNode*   node,
               const Field       field)
{
  SerdStatus        st         = SERD_SUCCESS;
  const SerdNode*   prefix     = NULL;
  SerdStringView    suffix     = {NULL, 0};
  const char* const node_str   = serd_node_string(node);
  const bool        has_scheme = serd_uri_string_has_scheme(node_str);

  if (supports_abbrev(writer)) {
    if (field == FIELD_PREDICATE && !strcmp(node_str, NS_RDF "type")) {
      return esink("a", 1, writer);
    }

    if (!strcmp(node_str, NS_RDF "nil")) {
      return esink("()", 2, writer);
    }

    if (has_scheme && (writer->flags & SERD_WRITE_CURIED) &&
        serd_env_qualify(writer->env, node, &prefix, &suffix) &&
        is_name(serd_node_string(prefix), serd_node_length(prefix)) &&
        is_name(suffix.data, suffix.length)) {
      TRY(st, write_uri_from_node(writer, prefix));
      TRY(st, esink(":", 1, writer));
      return ewrite_uri(writer, suffix.data, suffix.length);
    }
  }

  if (!has_scheme && !supports_uriref(writer) &&
      !serd_env_base_uri(writer->env)) {
    return w_err(writer,
                 SERD_BAD_ARG,
                 "syntax does not support URI reference <%s>\n",
                 node_str);
  }

  TRY(st, esink("<", 1, writer));

  if ((writer->flags & SERD_WRITE_RESOLVED) && serd_env_base_uri(writer->env)) {
    const SerdURIView  base_uri = serd_env_base_uri_view(writer->env);
    SerdURIView        uri      = serd_parse_uri(node_str);
    SerdURIView        abs_uri  = serd_resolve_uri(uri, base_uri);
    bool               rooted   = uri_is_under(&base_uri, &writer->root_uri);
    const SerdURIView* root     = rooted ? &writer->root_uri : &base_uri;
    UriSinkContext     ctx      = {writer, SERD_SUCCESS};

    if (!supports_abbrev(writer) || !uri_is_under(&abs_uri, root)) {
      serd_write_uri(abs_uri, uri_sink, &ctx);
    } else {
      serd_write_uri(serd_relative_uri(uri, base_uri), uri_sink, &ctx);
    }
  } else {
    TRY(st, write_uri_from_node(writer, node));
  }

  return esink(">", 1, writer);
}

SERD_NODISCARD static SerdStatus
write_curie(SerdWriter* const writer, const SerdNode* const node)
{
  const char* const node_str = serd_node_string(node);
  SerdStringView    prefix   = {NULL, 0};
  SerdStringView    suffix   = {NULL, 0};
  SerdStatus        st       = SERD_SUCCESS;

  // In fast-and-loose Turtle/TriG mode CURIEs are simply passed through
  const bool fast =
    !(writer->flags & (SERD_WRITE_CURIED | SERD_WRITE_RESOLVED));

  if (!supports_abbrev(writer) || !fast) {
    if ((st = serd_env_expand(writer->env, node, &prefix, &suffix))) {
      return w_err(writer, st, "undefined namespace prefix '%s'\n", node_str);
    }
  }

  if (!supports_abbrev(writer)) {
    TRY(st, esink("<", 1, writer));
    TRY(st, ewrite_uri(writer, prefix.data, prefix.length));
    TRY(st, ewrite_uri(writer, suffix.data, suffix.length));
    TRY(st, esink(">", 1, writer));
  } else {
    TRY(st, write_lname(writer, node_str, node->length));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_blank(SerdWriter* const        writer,
            const SerdNode*          node,
            const Field              field,
            const SerdStatementFlags flags)
{
  SerdStatus        st       = SERD_SUCCESS;
  const char* const node_str = serd_node_string(node);

  if (supports_abbrev(writer)) {
    if ((field == FIELD_SUBJECT && (flags & SERD_ANON_S_BEGIN)) ||
        (field == FIELD_OBJECT && (flags & SERD_ANON_O_BEGIN))) {
      return write_sep(writer, SEP_ANON_BEGIN);
    }

    if ((field == FIELD_SUBJECT && (flags & SERD_LIST_S_BEGIN)) ||
        (field == FIELD_OBJECT && (flags & SERD_LIST_O_BEGIN))) {
      return write_sep(writer, SEP_LIST_BEGIN);
    }

    if ((field == FIELD_SUBJECT && (flags & SERD_EMPTY_S)) ||
        (field == FIELD_OBJECT && (flags & SERD_EMPTY_O))) {
      return esink("[]", 2, writer);
    }
  }

  TRY(st, esink("_:", 2, writer));
  if (writer->bprefix &&
      !strncmp(node_str, writer->bprefix, writer->bprefix_len)) {
    TRY(st,
        esink(node_str + writer->bprefix_len,
              node->length - writer->bprefix_len,
              writer));
  } else {
    TRY(st, esink(node_str, node->length, writer));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_node(SerdWriter* const        writer,
           const SerdNode* const    node,
           const Field              field,
           const SerdStatementFlags flags)
{
  SerdStatus st = SERD_SUCCESS;

  switch (node->type) {
  case SERD_LITERAL:
    st = write_literal(writer, node, flags);
    break;
  case SERD_URI:
    st = write_uri_node(writer, node, field);
    break;
  case SERD_CURIE:
    st = write_curie(writer, node);
    break;
  case SERD_BLANK:
    st = write_blank(writer, node, field, flags);
    break;
  }

  if (node->type != SERD_BLANK) {
    writer->last_sep = SEP_NODE;
  }

  return st;
}

static bool
is_resource(const SerdNode* node)
{
  return node && node->type > SERD_LITERAL;
}

SERD_NODISCARD static SerdStatus
write_pred(SerdWriter* writer, SerdStatementFlags flags, const SerdNode* pred)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, write_node(writer, pred, FIELD_PREDICATE, flags));
  TRY(st, write_sep(writer, SEP_P_O));

  serd_node_set(&writer->context.predicate, pred);
  writer->context.predicates     = true;
  writer->context.comma_indented = false;
  return st;
}

SERD_NODISCARD static SerdStatus
write_list_next(SerdWriter* const        writer,
                const SerdStatementFlags flags,
                const SerdNode* const    predicate,
                const SerdNode* const    object)
{
  SerdStatus st = SERD_SUCCESS;

  if (!strcmp(serd_node_string(object), NS_RDF "nil")) {
    TRY(st, write_sep(writer, SEP_LIST_END));
    return SERD_FAILURE;
  }

  if (!strcmp(serd_node_string(predicate), NS_RDF "first")) {
    TRY(st, write_node(writer, object, FIELD_OBJECT, flags));
  } else {
    TRY(st, write_sep(writer, SEP_LIST_SEP));
  }

  return st;
}

static SerdStatus
terminate_context(SerdWriter* writer)
{
  SerdStatus st = SERD_SUCCESS;

  if (writer->context.subject && writer->context.subject->type) {
    TRY(st, write_sep(writer, SEP_END_S));
  }

  if (writer->context.graph && writer->context.graph->type) {
    TRY(st, write_sep(writer, SEP_GRAPH_END));
  }

  return st;
}

static SerdStatus
serd_writer_write_statement(SerdWriter* const     writer,
                            SerdStatementFlags    flags,
                            const SerdNode* const graph,
                            const SerdNode* const subject,
                            const SerdNode* const predicate,
                            const SerdNode* const object)
{
  SerdStatus st = SERD_SUCCESS;

  if (!is_resource(subject) || !is_resource(predicate) || !object) {
    return SERD_BAD_ARG;
  }

  if ((flags & SERD_LIST_O_BEGIN) &&
      !strcmp(serd_node_string(object), NS_RDF "nil")) {
    /* Tolerate LIST_O_BEGIN for "()" objects, even though it doesn't make
       much sense, because older versions handled this gracefully.  Consider
       making this an error in a later major version. */
    flags &= (SerdStatementFlags)~SERD_LIST_O_BEGIN;
  }

  // Simple case: write a line of NTriples or NQuads
  if (writer->syntax == SERD_NTRIPLES || writer->syntax == SERD_NQUADS) {
    TRY(st, write_node(writer, subject, FIELD_SUBJECT, flags));
    TRY(st, esink(" ", 1, writer));
    TRY(st, write_node(writer, predicate, FIELD_PREDICATE, flags));
    TRY(st, esink(" ", 1, writer));
    TRY(st, write_node(writer, object, FIELD_OBJECT, flags));
    if (writer->syntax == SERD_NQUADS && graph) {
      TRY(st, esink(" ", 1, writer));
      TRY(st, write_node(writer, graph, FIELD_GRAPH, flags));
    }
    TRY(st, esink(" .\n", 3, writer));
    return SERD_SUCCESS;
  }

  SERD_DISABLE_NULL_WARNINGS

  // Separate graphs if necessary
  if ((graph && !serd_node_equals(graph, writer->context.graph)) ||
      (!graph && ctx(writer, FIELD_GRAPH))) {
    TRY(st, terminate_context(writer));
    reset_context(writer, RESET_GRAPH | RESET_INDENT);
    if (graph) {
      TRY(st, write_newline(writer));
      TRY(st, write_node(writer, graph, FIELD_GRAPH, flags));
      TRY(st, write_sep(writer, SEP_GRAPH_BEGIN));
      serd_node_set(&writer->context.graph, graph);
    }
  }

  SERD_RESTORE_WARNINGS

  if ((flags & SERD_LIST_CONT)) {
    // Continue a list
    if (!strcmp(serd_node_string(predicate), NS_RDF "first") &&
        !strcmp(serd_node_string(object), NS_RDF "nil")) {
      return esink("()", 2, writer);
    }

    TRY_FAILING(st, write_list_next(writer, flags, predicate, object));
    if (st == SERD_FAILURE) { // Reached end of list
      pop_context(writer);
      return SERD_SUCCESS;
    }

  } else if (serd_node_equals(subject, writer->context.subject)) {
    if (serd_node_equals(predicate, writer->context.predicate)) {
      // Elide S P (write O)

      const Sep  last      = writer->last_sep;
      const bool anon_o    = flags & SERD_ANON_O_BEGIN;
      const bool list_o    = flags & SERD_LIST_O_BEGIN;
      const bool open_o    = anon_o || list_o;
      const bool after_end = (last == SEP_ANON_END) || (last == SEP_LIST_END);

      TRY(st,
          write_sep(writer,
                    after_end ? (open_o ? SEP_JOIN_O_AA : SEP_JOIN_O_AN)
                              : (open_o ? SEP_JOIN_O_NA : SEP_END_O)));

    } else {
      // Elide S (write P and O)

      if (writer->context.comma_indented) {
        --writer->indent;
        writer->context.comma_indented = false;
      }

      const bool first = !ctx(writer, FIELD_PREDICATE);
      TRY(st, write_sep(writer, first ? SEP_S_P : SEP_END_P));
      TRY(st, write_pred(writer, flags, predicate));
    }

    TRY(st, write_node(writer, object, FIELD_OBJECT, flags));

  } else {
    // No abbreviation
    if (serd_stack_is_empty(&writer->anon_stack)) {
      if (ctx(writer, FIELD_SUBJECT)) {
        TRY(st, write_sep(writer, SEP_END_S));
      }

      if (writer->last_sep == SEP_END_S || writer->last_sep == SEP_END_DIRECT) {
        TRY(st, write_newline(writer));
      }

      TRY(st, write_node(writer, subject, FIELD_SUBJECT, flags));
      if ((flags & (SERD_ANON_S_BEGIN | SERD_LIST_S_BEGIN))) {
        TRY(st, write_sep(writer, SEP_ANON_S_P));
      } else {
        TRY(st, write_sep(writer, SEP_S_P));
      }

    } else {
      TRY(st, write_sep(writer, SEP_ANON_S_P));
    }

    reset_context(writer, 0U);
    serd_node_set(&writer->context.subject, subject);

    if (!(flags & SERD_LIST_S_BEGIN)) {
      TRY(st, write_pred(writer, flags, predicate));
    }

    TRY(st, write_node(writer, object, FIELD_OBJECT, flags));
  }

  if (flags & (SERD_ANON_S_BEGIN | SERD_LIST_S_BEGIN)) {
    // Push context for anonymous or list subject
    const bool is_list = (flags & SERD_LIST_S_BEGIN);
    TRY(st,
        push_context(writer,
                     is_list ? CTX_LIST : CTX_BLANK,
                     graph,
                     subject,
                     is_list ? NULL : predicate));
  }

  if (flags & (SERD_ANON_O_BEGIN | SERD_LIST_O_BEGIN)) {
    // Push context for anonymous or list object if necessary
    TRY(st,
        push_context(writer,
                     (flags & SERD_LIST_O_BEGIN) ? CTX_LIST : CTX_BLANK,
                     graph,
                     object,
                     NULL));
  }

  return st;
}

static SerdStatus
serd_writer_end_anon(SerdWriter* writer, const SerdNode* node)
{
  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax == SERD_NTRIPLES || writer->syntax == SERD_NQUADS) {
    return SERD_SUCCESS;
  }

  if (serd_stack_is_empty(&writer->anon_stack)) {
    return w_err(writer, SERD_BAD_CALL, "unexpected end of anonymous node\n");
  }

  // Write the end separator ']' and pop the context
  TRY(st, write_sep(writer, SEP_ANON_END));
  pop_context(writer);

  if (writer->context.predicate &&
      serd_node_equals(node, writer->context.subject)) {
    // Now-finished anonymous node is the new subject with no other context
    memset(writer->context.predicate, 0, sizeof(SerdNode));
  }

  return st;
}

SerdStatus
serd_writer_finish(SerdWriter* writer)
{
  const SerdStatus st0 = terminate_context(writer);
  const SerdStatus st1 = serd_byte_sink_flush(&writer->byte_sink);
  free_anon_stack(writer);
  reset_context(writer, RESET_GRAPH | RESET_INDENT);
  return st0 ? st0 : st1;
}

SerdWriter*
serd_writer_new(SerdSyntax      syntax,
                SerdWriterFlags flags,
                SerdEnv*        env,
                SerdWriteFunc   ssink,
                void*           stream)
{
  const WriteContext context = WRITE_CONTEXT_NULL;
  SerdWriter*        writer  = (SerdWriter*)calloc(1, sizeof(SerdWriter));

  writer->syntax     = syntax;
  writer->flags      = flags;
  writer->env        = env;
  writer->root_node  = NULL;
  writer->root_uri   = SERD_URI_NULL;
  writer->anon_stack = serd_stack_new(SERD_PAGE_SIZE);
  writer->context    = context;
  writer->byte_sink  = serd_byte_sink_new(
    ssink, stream, (flags & SERD_WRITE_BULK) ? SERD_PAGE_SIZE : 1);

  writer->iface.handle    = writer;
  writer->iface.base      = (SerdBaseFunc)serd_writer_set_base_uri;
  writer->iface.prefix    = (SerdPrefixFunc)serd_writer_set_prefix;
  writer->iface.statement = (SerdStatementFunc)serd_writer_write_statement;
  writer->iface.end       = (SerdEndFunc)serd_writer_end_anon;

  return writer;
}

void
serd_writer_set_error_sink(SerdWriter*   writer,
                           SerdErrorFunc error_func,
                           void*         error_handle)
{
  writer->error_func   = error_func;
  writer->error_handle = error_handle;
}

void
serd_writer_chop_blank_prefix(SerdWriter* writer, const char* prefix)
{
  free(writer->bprefix);
  writer->bprefix_len = 0;
  writer->bprefix     = NULL;

  const size_t prefix_len = prefix ? strlen(prefix) : 0;
  if (prefix_len) {
    writer->bprefix_len = prefix_len;
    writer->bprefix     = (char*)malloc(writer->bprefix_len + 1);
    memcpy(writer->bprefix, prefix, writer->bprefix_len + 1);
  }
}

SerdStatus
serd_writer_set_base_uri(SerdWriter* writer, const SerdNode* uri)
{
  SERD_DISABLE_NULL_WARNINGS

  if (uri && serd_node_type(uri) != SERD_URI) {
    return SERD_BAD_ARG;
  }

  if (serd_node_equals(serd_env_base_uri(writer->env), uri)) {
    return SERD_SUCCESS;
  }

  const SerdStringView uri_string =
    uri ? serd_node_string_view(uri) : serd_empty_string();

  SerdStatus st = SERD_SUCCESS;
  TRY(st, serd_env_set_base_uri(writer->env, uri_string));

  if (uri && (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG)) {
    TRY(st, terminate_context(writer));
    TRY(st, esink("@base <", 7, writer));
    TRY(st, esink(uri_string.data, uri_string.length, writer));
    TRY(st, esink(">", 1, writer));
    writer->last_sep = SEP_NODE;
    TRY(st, write_sep(writer, SEP_END_DIRECT));
  }

  SERD_RESTORE_WARNINGS

  return reset_context(writer, RESET_GRAPH | RESET_INDENT);
}

SerdStatus
serd_writer_set_root_uri(SerdWriter* writer, const SerdNode* uri)
{
  serd_node_free(writer->root_node);
  writer->root_node = NULL;
  writer->root_uri  = SERD_URI_NULL;

  if (uri) {
    writer->root_node = serd_node_copy(uri);
    writer->root_uri  = serd_node_uri_view(writer->root_node);
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st,
      serd_env_set_prefix(
        writer->env, serd_node_string_view(name), serd_node_string_view(uri)));

  if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
    TRY(st, terminate_context(writer));
    TRY(st, esink("@prefix ", 8, writer));
    TRY(st, esink(serd_node_string(name), name->length, writer));
    TRY(st, esink(": <", 3, writer));
    TRY(st, ewrite_uri(writer, serd_node_string(uri), uri->length));
    TRY(st, esink(">", 1, writer));
    writer->last_sep = SEP_NODE;
    TRY(st, write_sep(writer, SEP_END_DIRECT));
  }

  return reset_context(writer, RESET_GRAPH | RESET_INDENT);
}

void
serd_writer_free(SerdWriter* writer)
{
  if (!writer) {
    return;
  }

  SERD_DISABLE_NULL_WARNINGS
  serd_writer_finish(writer);
  SERD_RESTORE_WARNINGS
  free_context(&writer->context);
  free_anon_stack(writer);
  serd_stack_free(&writer->anon_stack);
  free(writer->bprefix);
  serd_byte_sink_free(&writer->byte_sink);
  serd_node_free(writer->root_node);
  free(writer);
}

const SerdSink*
serd_writer_sink(SerdWriter* writer)
{
  return &writer->iface;
}

size_t
serd_buffer_sink(const void* const buf,
                 const size_t      size,
                 const size_t      nmemb,
                 void* const       stream)
{
  assert(size == 1);
  (void)size;

  SerdBuffer* buffer  = (SerdBuffer*)stream;
  char*       new_buf = (char*)realloc(buffer->buf, buffer->len + nmemb);
  if (new_buf) {
    memcpy(new_buf + buffer->len, buf, nmemb);
    buffer->buf = new_buf;
    buffer->len += nmemb;
  }
  return nmemb;
}

char*
serd_buffer_sink_finish(SerdBuffer* const stream)
{
  serd_buffer_sink("", 1, 1, stream);
  return (char*)stream->buf;
}
