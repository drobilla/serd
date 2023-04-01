// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "block_dumper.h"
#include "env.h"
#include "memory.h"
#include "namespaces.h"
#include "node.h"
#include "sink.h"
#include "string_utils.h"
#include "system.h"
#include "try.h"
#include "turtle.h"
#include "uri_utils.h"
#include "warnings.h"
#include "world.h"

#include "serd/attributes.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/uri.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/string_view.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
  CTX_NAMED, ///< Normal non-anonymous context
  CTX_BLANK, ///< Anonymous blank node
  CTX_LIST,  ///< Anonymous list
} ContextType;

typedef struct {
  ContextType        type;
  SerdStatementFlags flags;
  SerdNode*          graph;
  SerdNode*          subject;
  SerdNode*          predicate;
  bool               predicates;
  bool               comma_indented;
} WriteContext;

static const WriteContext WRITE_CONTEXT_NULL =
  {CTX_NAMED, 0U, NULL, NULL, NULL, 0U, 0U};

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
  SEP_LIST_SEP,    ///< List separator (newline)
  SEP_LIST_END,    ///< End of list (')')
  SEP_TLIST_BEGIN, ///< Start of terse list ('(')
  SEP_TLIST_SEP,   ///< Terse list separator (space)
  SEP_TLIST_END,   ///< End of terse list (')')
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
  {'.', +0, SEP_EACH, SEP_NONE, SEP_NONE},
  {'.', +0, SEP_EACH, SEP_NONE, SEP_NONE},
  {';', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_EACH},
  {',', +0, SEP_EACH, SEP_NONE, SEP_NONE},
  {NIL, +1, SEP_NONE, SEP_NONE, SEP_EACH},
  {' ', +0, SEP_NONE, SEP_NONE, SEP_NONE},
  {'[', +1, M(SEP_JOIN_O_AA), M(SEP_TLIST_BEGIN) | M(SEP_TLIST_SEP), SEP_NONE},
  {NIL, +1, SEP_NONE, SEP_NONE, M(SEP_ANON_BEGIN)},
  {']', -1, SEP_NONE, ~M(SEP_ANON_BEGIN), SEP_NONE},
  {'(', +1, M(SEP_JOIN_O_AA), SEP_NONE, SEP_EACH},
  {NIL, +0, SEP_NONE, SEP_EACH, SEP_NONE},
  {')', -1, SEP_NONE, SEP_EACH, SEP_NONE},
  {'(', +1, SEP_NONE, SEP_NONE, SEP_NONE},
  {NIL, +0, SEP_EACH, SEP_NONE, SEP_NONE},
  {')', -1, SEP_NONE, SEP_NONE, SEP_NONE},
  {'{', +1, SEP_EACH, SEP_NONE, SEP_EACH},
  {'}', -1, SEP_NONE, SEP_NONE, SEP_EACH},
};

#undef NIL
#undef M
#undef SEP_EACH

struct SerdWriterImpl {
  SerdWorld*      world;
  SerdSink        iface;
  SerdSyntax      syntax;
  SerdWriterFlags flags;
  SerdEnv*        env;
  SerdNode*       root_node;
  SerdURIView     root_uri;
  WriteContext*   anon_stack;
  size_t          max_depth;
  size_t          anon_stack_size;
  SerdBlockDumper output;
  WriteContext    context;
  Sep             last_sep;
  int             indent;
};

typedef enum { WRITE_STRING, WRITE_LONG_STRING } TextContext;
typedef enum { RESET_GRAPH = 1U << 0U, RESET_INDENT = 1U << 1U } ResetFlag;

SERD_NODISCARD static SerdStatus
serd_writer_set_base_uri(SerdWriter* writer, const SerdNode* uri);

SERD_NODISCARD static SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri);

SERD_NODISCARD static SerdStatus
write_node(SerdWriter*        writer,
           const SerdNode*    node,
           SerdField          field,
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
free_context(SerdWriter* const writer)
{
  serd_node_free(writer->world->allocator, writer->context.graph);
  serd_node_free(writer->world->allocator, writer->context.subject);
  serd_node_free(writer->world->allocator, writer->context.predicate);
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

  serd_world_verrorf(writer->world, st, fmt, args);

  va_end(args);
  return st;
}

static inline SerdNode*
ctx(SerdWriter* writer, const SerdField field)
{
  SerdNode* node = (field == SERD_SUBJECT)     ? writer->context.subject
                   : (field == SERD_PREDICATE) ? writer->context.predicate
                   : (field == SERD_GRAPH)     ? writer->context.graph
                                               : NULL;

  return node && node->type ? node : NULL;
}

SERD_NODISCARD static SerdStatus
push_context(SerdWriter* const        writer,
             const ContextType        type,
             const SerdStatementFlags flags,
             const SerdNode* const    graph,
             const SerdNode* const    subject,
             const SerdNode* const    predicate)
{
  // Push the current context to the stack

  if (writer->anon_stack_size >= writer->max_depth) {
    return SERD_BAD_STACK;
  }

  writer->anon_stack[writer->anon_stack_size++] = writer->context;

  // Update the current context

  const WriteContext current = {
    type,
    flags,
    serd_node_copy(writer->world->allocator, graph),
    serd_node_copy(writer->world->allocator, subject),
    serd_node_copy(writer->world->allocator, predicate),
    0U,
    0U};

  writer->context = current;
  return SERD_SUCCESS;
}

static void
pop_context(SerdWriter* writer)
{
  assert(writer->anon_stack_size > 0);

  free_context(writer);
  writer->context = writer->anon_stack[--writer->anon_stack_size];
}

SERD_NODISCARD static size_t
sink(const void* buf, size_t len, SerdWriter* writer)
{
  const size_t written = serd_block_dumper_write(buf, 1, len, &writer->output);

  if (written != len) {
    if (errno) {
      char message[1024] = {0};
      serd_system_strerror(errno, message, sizeof(message));

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

// Write a single character as a Unicode escape
// (Caller prints any single byte characters that don't need escaping)
static size_t
write_character(SerdWriter* const    writer,
                const uint8_t* const utf8,
                uint8_t* const       size,
                SerdStatus* const    st)
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
    uint8_t size = 0;
    len += write_character(writer, (const uint8_t*)utf8 + i, &size, st);
    i += size;
    if (*st && !(writer->flags & SERD_WRITE_LAX)) {
      break;
    }

    if (size == 0) {
      // Corrupt input, write percent-encoded bytes and scan to next start
      char escape[4] = {0, 0, 0, 0};
      for (; i < n_bytes && !is_utf8_leading((uint8_t)utf8[i]); ++i) {
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

  return (st == SERD_BAD_WRITE || !(writer->flags & SERD_WRITE_LAX))
           ? st
           : SERD_SUCCESS;
}

SERD_NODISCARD static SerdStatus
write_uri_from_node(SerdWriter* writer, const SerdNode* node)
{
  return ewrite_uri(writer, serd_node_string(node), serd_node_length(node));
}

SERD_NODISCARD static SerdStatus
write_utf8_percent_escape(SerdWriter* const writer,
                          const char* const utf8,
                          const size_t      n_bytes)
{
  static const char hex_chars[] = "0123456789ABCDEF";

  SerdStatus st        = SERD_SUCCESS;
  char       escape[4] = {'%', 0, 0, 0};

  for (size_t i = 0U; i < n_bytes; ++i) {
    const uint8_t byte = (uint8_t)utf8[i];
    escape[1]          = hex_chars[byte >> 4U];
    escape[2]          = hex_chars[byte & 0x0FU];

    TRY(st, esink(escape, 3, writer));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_PN_LOCAL_ESC(SerdWriter* const writer, const char c)
{
  const char buf[2] = {'\\', c};

  return esink(buf, sizeof(buf), writer);
}

SERD_NODISCARD static SerdStatus
write_lname_escape(SerdWriter* writer, const char* const utf8, size_t n_bytes)
{
  return is_PN_LOCAL_ESC(utf8[0])
           ? write_PN_LOCAL_ESC(writer, utf8[0])
           : write_utf8_percent_escape(writer, utf8, n_bytes);
}

SERD_NODISCARD static SerdStatus
write_lname(SerdWriter* writer, const char* utf8, const size_t n_bytes)
{
  SerdStatus st = SERD_SUCCESS;

  /* Thanks to the horribly complicated Turtle grammar for prefixed names,
     making sure we never write an invalid character is tedious.  We need to
     handle the first and last characters separately since they have different
     sets of valid characters. */

  // Write first character
  uint8_t   first_size = 0U;
  const int first = (int)parse_utf8_char((const uint8_t*)utf8, &first_size);
  if (is_PN_CHARS_U(first) || first == ':' || is_digit(first)) {
    TRY(st, esink(utf8, first_size, writer));
  } else {
    TRY(st, write_lname_escape(writer, utf8, first_size));
  }

  // Write middle and last characters
  for (size_t i = first_size; i < n_bytes;) {
    uint8_t   c_size = 0U;
    const int c      = (int)parse_utf8_char((const uint8_t*)utf8 + i, &c_size);

    if (is_PN_CHARS(c) || c == ':' || (c == '.' && (i + 1U < n_bytes))) {
      TRY(st, esink(&utf8[i], c_size, writer));
    } else {
      TRY(st, write_lname_escape(writer, &utf8[i], c_size));
    }

    i += c_size;
  }

  return st;
}

SERD_NODISCARD static size_t
write_long_string_escape(SerdWriter* const writer,
                         const size_t      n_consecutive_quotes,
                         const bool        is_last,
                         const char        c)
{
  switch (c) {
  case '\\':
    return sink("\\\\", 2, writer);

  case '\b':
    return sink("\\b", 2, writer);

  case '\n':
  case '\r':
  case '\t':
  case '\f':
    return sink(&c, 1, writer); // Write character as-is

  case '\"':
    if (n_consecutive_quotes >= 3 || is_last) {
      // Two quotes in a row, or quote at string end, escape
      return sink("\\\"", 2, writer);
    }

    return sink(&c, 1, writer);

  default:
    break;
  }

  return 0;
}

SERD_NODISCARD static size_t
write_short_string_escape(SerdWriter* const writer, const char c)
{
  switch (c) {
  case '\\':
    return sink("\\\\", 2, writer);
  case '\n':
    return sink("\\n", 2, writer);
  case '\r':
    return sink("\\r", 2, writer);
  case '\t':
    return sink("\\t", 2, writer);
  case '"':
    return sink("\\\"", 2, writer);
  default:
    break;
  }

  if (writer->syntax == SERD_TURTLE) {
    switch (c) {
    case '\b':
      return sink("\\b", 2, writer);
    case '\f':
      return sink("\\f", 2, writer);
    default:
      break;
    }
  }

  return 0;
}

static bool
text_must_escape(const char c)
{
  return c == '\\' || c == '"' || !in_range(c, 0x20, 0x7E);
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

    // Scan for the longest chunk of characters that can be written directly
    size_t j = i;
    for (; j < n_bytes && !text_must_escape(utf8[j]); ++j) {
    }

    // Write chunk as a single fast bulk write
    st = esink(&utf8[i], j - i, writer);
    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    // Try to write character as a special short escape (newline and friends)
    const char in         = utf8[i++];
    size_t     escape_len = 0;
    if (ctx == WRITE_LONG_STRING) {
      n_consecutive_quotes = (in == '\"') ? (n_consecutive_quotes + 1) : 0;
      escape_len           = write_long_string_escape(
        writer, n_consecutive_quotes, i == n_bytes, in);
    } else {
      escape_len = write_short_string_escape(writer, in);
    }

    if (escape_len == 0) {
      // No special escape for this character, write full Unicode escape
      uint8_t size = 0;
      write_character(writer, (const uint8_t*)utf8 + i - 1, &size, &st);
      if (st && !(writer->flags & SERD_WRITE_LAX)) {
        return st;
      }

      if (size == 0) {
        // Corrupt input, write replacement character and scan to the next start
        st = esink(replacement_char, sizeof(replacement_char), writer);
        for (; i < n_bytes && !is_utf8_leading((uint8_t)utf8[i]); ++i) {
        }
      } else {
        i += size - 1U;
      }
    }
  }

  return SERD_SUCCESS;
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
write_newline(SerdWriter* writer, bool terse)
{
  SerdStatus st = SERD_SUCCESS;

  if (terse || (writer->flags & SERD_WRITE_TERSE)) {
    return esink(" ", 1, writer);
  }

  TRY(st, esink("\n", 1, writer));
  for (int i = 0; i < writer->indent; ++i) {
    TRY(st, esink("\t", 1, writer));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_top_level_sep(SerdWriter* writer)
{
  return (writer->last_sep && !(writer->flags & SERD_WRITE_TERSE))
           ? write_newline(writer, false)
           : SERD_SUCCESS;
}

SERD_NODISCARD static SerdStatus
write_sep(SerdWriter* writer, const SerdStatementFlags flags, Sep sep)
{
  SerdStatus           st   = SERD_SUCCESS;
  const SepRule* const rule = &rules[sep];

  const bool pre_line  = (rule->pre_line_after & (1U << writer->last_sep));
  const bool post_line = (rule->post_line_after & (1U << writer->last_sep));

  const bool terse = (((flags & SERD_TERSE_S) && (flags & SERD_LIST_S)) ||
                      ((flags & SERD_TERSE_O) && (flags & SERD_LIST_O)));

  if (terse && sep >= SEP_LIST_BEGIN && sep <= SEP_LIST_END) {
    sep = (Sep)((int)sep + 3); // Switch to corresponding terse separator
  }

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
    TRY(st, write_newline(writer, terse));
  } else if (rule->pre_space_after & (1U << writer->last_sep)) {
    TRY(st, esink(" ", 1, writer));
  }

  // Write actual separator string
  if (rule->sep) {
    TRY(st, esink(&rule->sep, 1, writer));
  }

  // Write newline after separator if necessary
  if (post_line) {
    TRY(st, write_newline(writer, terse));
    if (rule->post_line_after != ~(SepMask)0U) {
      writer->last_sep = SEP_NEWLINE;
    }
  }

  // Reset context and write a blank line after ends of subjects
  if (sep == SEP_END_S || sep == SEP_END_DIRECT) {
    writer->indent                 = ctx(writer, SERD_GRAPH) ? 1 : 0;
    writer->context.predicates     = false;
    writer->context.comma_indented = false;
    if (!terse) {
      TRY(st, esink("\n", 1, writer));
    }
  }

  writer->last_sep = sep;
  return st;
}

static void
free_anon_stack(SerdWriter* writer)
{
  while (writer->anon_stack_size > 0) {
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

  writer->anon_stack_size        = 0;
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

  if (supports_abbrev(writer) && (node->flags & SERD_IS_LONG)) {
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
    return write_node(writer, datatype, (SerdField)-1, flags);
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_full_uri_node(SerdWriter* const writer, const SerdNode* const node)
{
  SerdStatus st       = SERD_SUCCESS;
  const bool verbatim = (writer->flags & SERD_WRITE_VERBATIM);

  if (verbatim || !serd_env_base_uri(writer->env)) {
    // Resolution disabled or we have no base URI, simply write the node
    TRY(st, esink("<", 1, writer));
    TRY(st, write_uri_from_node(writer, node));
    TRY(st, esink(">", 1, writer));
    return SERD_SUCCESS;
  }

  // Resolve the input node URI reference to a (hopefully) absolute URI
  const SerdURIView base_uri = serd_env_base_uri_view(writer->env);
  SerdURIView       uri      = serd_parse_uri(serd_node_string(node));
  SerdURIView       abs_uri  = serd_resolve_uri(uri, base_uri);

  // Determine if we should write the absolute URI or make it relative again
  const bool         base_rooted = uri_is_under(&base_uri, &writer->root_uri);
  const SerdURIView* root        = base_rooted ? &writer->root_uri : &base_uri;
  const bool         rooted      = uri_is_under(&abs_uri, root);
  const bool         write_abs   = !supports_abbrev(writer) || !rooted;

  TRY(st, esink("<", 1, writer));

  UriSinkContext context = {writer, SERD_SUCCESS};
  if (write_abs) {
    serd_write_uri(abs_uri, uri_sink, &context);
  } else {
    serd_write_uri(serd_relative_uri(uri, base_uri), uri_sink, &context);
  }

  return esink(">", 1, writer);
}

SERD_NODISCARD static SerdStatus
write_uri_node(SerdWriter* const     writer,
               const SerdNode* const node,
               const SerdField       field)
{
  SerdStatus        st         = SERD_SUCCESS;
  const char* const node_str   = serd_node_string(node);
  const bool        has_scheme = serd_uri_string_has_scheme(node_str);

  if (supports_abbrev(writer)) {
    const SerdNode* prefix_node = NULL;
    ZixStringView   suffix      = {NULL, 0};
    if (field == SERD_PREDICATE && !strcmp(node_str, NS_RDF "type")) {
      return esink("a", 1, writer);
    }

    if (!strcmp(node_str, NS_RDF "nil")) {
      return esink("()", 2, writer);
    }

    if (has_scheme && !(writer->flags & SERD_WRITE_EXPANDED) &&
        serd_env_qualify_in_place(writer->env, node, &prefix_node, &suffix)) {
      const ZixStringView prefix = serd_node_string_view(prefix_node);
      TRY(st, write_lname(writer, prefix.data, prefix.length));
      TRY(st, esink(":", 1, writer));
      return write_lname(writer, suffix.data, suffix.length);
    }
  }

  if (!has_scheme && !supports_uriref(writer) &&
      !serd_env_base_uri(writer->env)) {
    return w_err(writer,
                 SERD_BAD_ARG,
                 "syntax does not support URI reference <%s>\n",
                 node_str);
  }

  return write_full_uri_node(writer, node);
}

SERD_NODISCARD static SerdStatus
write_curie(SerdWriter* const writer, const SerdNode* const node)
{
  const char* const node_str = serd_node_string(node);
  ZixStringView     prefix   = {NULL, 0};
  ZixStringView     suffix   = {NULL, 0};
  SerdStatus        st       = SERD_SUCCESS;

  // In verbatim Turtle/TriG mode, CURIEs are simply passed through
  const bool fast = (writer->flags & SERD_WRITE_VERBATIM);

  if (!supports_abbrev(writer) || !fast) {
    const ZixStringView curie = serd_node_string_view(node);
    if ((st = serd_env_expand_in_place(writer->env, curie, &prefix, &suffix))) {
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
            const SerdField          field,
            const SerdStatementFlags flags)
{
  SerdStatus        st       = SERD_SUCCESS;
  const char* const node_str = serd_node_string(node);

  if (supports_abbrev(writer)) {
    if ((field == SERD_SUBJECT && (flags & SERD_ANON_S)) ||
        (field == SERD_OBJECT && (flags & SERD_ANON_O))) {
      return write_sep(writer, flags, SEP_ANON_BEGIN);
    }

    if ((field == SERD_SUBJECT && (flags & SERD_LIST_S)) ||
        (field == SERD_OBJECT && (flags & SERD_LIST_O))) {
      return write_sep(writer, flags, SEP_LIST_BEGIN);
    }

    if ((field == SERD_SUBJECT && (flags & SERD_EMPTY_S)) ||
        (field == SERD_OBJECT && (flags & SERD_EMPTY_O))) {
      return esink("[]", 2, writer);
    }
  }

  TRY(st, esink("_:", 2, writer));
  return esink(node_str, node->length, writer);
}

SERD_NODISCARD static SerdStatus
write_variable(SerdWriter* const writer, const SerdNode* const node)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, esink("?", 1, writer));
  TRY(st, esink(serd_node_string(node), node->length, writer));

  writer->last_sep = SEP_NONE;
  return st;
}

SERD_NODISCARD static SerdStatus
write_node(SerdWriter* const        writer,
           const SerdNode* const    node,
           const SerdField          field,
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
  case SERD_VARIABLE:
    st = write_variable(writer, node);
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

  TRY(st, write_node(writer, pred, SERD_PREDICATE, flags));
  TRY(st, write_sep(writer, flags, SEP_P_O));

  writer->context.predicates     = true;
  writer->context.comma_indented = false;
  return serd_node_set(
    writer->world->allocator, &writer->context.predicate, pred);
}

SERD_NODISCARD static SerdStatus
write_list_next(SerdWriter* const        writer,
                const SerdStatementFlags flags,
                const SerdNode* const    predicate,
                const SerdNode* const    object)
{
  SerdStatus st = SERD_SUCCESS;

  if (!strcmp(serd_node_string(object), NS_RDF "nil")) {
    TRY(st, write_sep(writer, writer->context.flags, SEP_LIST_END));
    return SERD_FAILURE;
  }

  if (!strcmp(serd_node_string(predicate), NS_RDF "first")) {
    TRY(st, write_node(writer, object, SERD_OBJECT, flags));
  } else {
    TRY(st, write_sep(writer, writer->context.flags, SEP_LIST_SEP));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
terminate_context(SerdWriter* writer)
{
  SerdStatus st = SERD_SUCCESS;

  if (ctx(writer, SERD_SUBJECT)) {
    TRY(st, write_sep(writer, writer->context.flags, SEP_END_S));
  }

  if (ctx(writer, SERD_GRAPH)) {
    TRY(st, write_sep(writer, writer->context.flags, SEP_GRAPH_END));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_ntriples_statement(SerdWriter* const        writer,
                         const SerdStatementFlags flags,
                         const SerdNode* const    subject,
                         const SerdNode* const    predicate,
                         const SerdNode* const    object)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, write_node(writer, subject, SERD_SUBJECT, flags));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_node(writer, predicate, SERD_PREDICATE, flags));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_node(writer, object, SERD_OBJECT, flags));
  TRY(st, esink(" .\n", 3, writer));

  return st;
}

static SerdStatus
write_nquads_statement(SerdWriter* const        writer,
                       const SerdStatementFlags flags,
                       const SerdNode* const    subject,
                       const SerdNode* const    predicate,
                       const SerdNode* const    object,
                       const SerdNode* const    graph)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, write_node(writer, subject, SERD_SUBJECT, flags));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_node(writer, predicate, SERD_PREDICATE, flags));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_node(writer, object, SERD_OBJECT, flags));

  if (graph) {
    TRY(st, esink(" ", 1, writer));
    TRY(st, write_node(writer, graph, SERD_GRAPH, flags));
  }

  return esink(" .\n", 3, writer);
}

static SerdStatus
update_abbreviation_context(SerdWriter* const        writer,
                            const SerdStatementFlags flags,
                            const SerdNode* const    subject,
                            const SerdNode* const    predicate,
                            const SerdNode* const    object,
                            const SerdNode* const    graph)
{
  SerdStatus st = SERD_SUCCESS;

  // Push context for list or anonymous subject if necessary
  if (flags & SERD_ANON_S) {
    st = push_context(writer, CTX_BLANK, flags, graph, subject, predicate);
  } else if (flags & SERD_LIST_S) {
    st = push_context(writer, CTX_LIST, flags, graph, subject, NULL);
  }

  // Push context for list or anonymous object if necessary
  if (!st) {
    if (flags & SERD_ANON_O) {
      st = push_context(writer, CTX_BLANK, flags, graph, object, NULL);
    } else if (flags & SERD_LIST_O) {
      st = push_context(writer, CTX_LIST, flags, graph, object, NULL);
    }
  }

  return st;
}

SERD_NODISCARD static SerdStatus
write_list_statement(SerdWriter* const        writer,
                     const SerdStatementFlags flags,
                     const SerdNode* const    subject,
                     const SerdNode* const    predicate,
                     const SerdNode* const    object,
                     const SerdNode* const    graph)
{
  SerdStatus st = SERD_SUCCESS;

  if (!strcmp(serd_node_string(predicate), NS_RDF "first") &&
      !strcmp(serd_node_string(object), NS_RDF "nil")) {
    return esink("()", 2, writer);
  }

  TRY_FAILING(st, write_list_next(writer, flags, predicate, object));
  if (st == SERD_FAILURE) {
    pop_context(writer);
    return SERD_SUCCESS;
  }

  return update_abbreviation_context(
    writer, flags, subject, predicate, object, graph);
}

SERD_NODISCARD static SerdStatus
write_turtle_trig_statement(SerdWriter* const     writer,
                            SerdStatementFlags    flags,
                            const SerdNode* const subject,
                            const SerdNode* const predicate,
                            const SerdNode* const object,
                            const SerdNode* const graph)
{
  SerdStatus st = SERD_SUCCESS;

  if ((flags & SERD_LIST_O) &&
      !strcmp(serd_node_string(object), NS_RDF "nil")) {
    /* Tolerate LIST_O_BEGIN for "()" objects, even though it doesn't make
       much sense, because older versions handled this gracefully.  Consider
       making this an error in a later major version. */
    flags &= (SerdStatementFlags)~SERD_LIST_O;
  }

  if (writer->context.type == CTX_LIST) {
    return write_list_statement(
      writer, flags, subject, predicate, object, graph);
  }

  if (serd_node_equals(subject, writer->context.subject)) {
    if (serd_node_equals(predicate, writer->context.predicate)) {
      // Elide S P (write O)

      const Sep  last      = writer->last_sep;
      const bool anon_o    = flags & SERD_ANON_O;
      const bool list_o    = flags & SERD_LIST_O;
      const bool open_o    = anon_o || list_o;
      const bool after_end = (last == SEP_ANON_END) || (last == SEP_LIST_END);

      TRY(st,
          write_sep(writer,
                    flags,
                    after_end ? (open_o ? SEP_JOIN_O_AA : SEP_JOIN_O_AN)
                              : (open_o ? SEP_JOIN_O_NA : SEP_END_O)));

    } else {
      // Elide S (write P and O)

      if (writer->context.comma_indented && !(flags & SERD_ANON_S)) {
        --writer->indent;
        writer->context.comma_indented = false;
      }

      const bool first = !ctx(writer, SERD_PREDICATE);
      TRY(st, write_sep(writer, flags, first ? SEP_S_P : SEP_END_P));
      TRY(st, write_pred(writer, flags, predicate));
    }

  } else {
    // No abbreviation

    if (ctx(writer, SERD_SUBJECT)) {
      TRY(st, write_sep(writer, flags, SEP_END_S));
    }

    if (writer->last_sep == SEP_END_S || writer->last_sep == SEP_END_DIRECT) {
      TRY(st, write_top_level_sep(writer));
    }

    // Write subject node
    TRY(st, write_node(writer, subject, SERD_SUBJECT, flags));
    if (!(flags & (SERD_ANON_S | SERD_LIST_S))) {
      TRY(st, write_sep(writer, flags, SEP_S_P));
    } else if (flags & SERD_ANON_S) {
      TRY(st, write_sep(writer, flags, SEP_ANON_S_P));
    }

    // Set context to new subject
    reset_context(writer, 0U);
    TRY(st,
        serd_node_set(
          writer->world->allocator, &writer->context.subject, subject));

    // Write predicate
    if (!(flags & SERD_LIST_S)) {
      TRY(st, write_pred(writer, flags, predicate));
    }
  }

  TRY(st, write_node(writer, object, SERD_OBJECT, flags));

  return update_abbreviation_context(
    writer, flags, subject, predicate, object, graph);
}

SERD_NODISCARD static SerdStatus
write_turtle_statement(SerdWriter* const        writer,
                       const SerdStatementFlags flags,
                       const SerdNode* const    subject,
                       const SerdNode* const    predicate,
                       const SerdNode* const    object)
{
  return write_turtle_trig_statement(
    writer, flags, subject, predicate, object, NULL);
}

SERD_NODISCARD static SerdStatus
write_trig_statement(SerdWriter* const        writer,
                     const SerdStatementFlags flags,
                     const SerdNode* const    subject,
                     const SerdNode* const    predicate,
                     const SerdNode* const    object,
                     const SerdNode* const    graph)
{
  SerdStatus st = SERD_SUCCESS;

  if (!serd_node_equals(graph, writer->context.graph)) {
    TRY(st, terminate_context(writer));
    TRY(st, write_top_level_sep(writer));
    reset_context(writer, true);

    if (graph) {
      TRY(st, write_node(writer, graph, SERD_GRAPH, flags));
      TRY(st, write_sep(writer, flags, SEP_GRAPH_BEGIN));
      serd_node_set(writer->world->allocator, &writer->context.graph, graph);
    }
  }

  return write_turtle_trig_statement(
    writer, flags, subject, predicate, object, graph);
}

SERD_NODISCARD static SerdStatus
serd_writer_write_statement(SerdWriter* const          writer,
                            const SerdStatementFlags   flags,
                            const SerdStatement* const statement)
{
  const SerdNode* const subject   = serd_statement_subject(statement);
  const SerdNode* const predicate = serd_statement_predicate(statement);
  const SerdNode* const object    = serd_statement_object(statement);
  const SerdNode* const graph     = serd_statement_graph(statement);

  if (!is_resource(subject) || !is_resource(predicate) || !object ||
      ((flags & SERD_ANON_S) && (flags & SERD_LIST_S)) ||  // Nonsense
      ((flags & SERD_ANON_O) && (flags & SERD_LIST_O)) ||  // Nonsense
      ((flags & SERD_ANON_S) && (flags & SERD_TERSE_S)) || // Unsupported
      ((flags & SERD_ANON_O) && (flags & SERD_TERSE_O))) { // Unsupported
    return SERD_BAD_ARG;
  }

  switch (writer->syntax) {
  case SERD_SYNTAX_EMPTY:
    break;

  case SERD_TURTLE:
    return write_turtle_statement(writer, flags, subject, predicate, object);

  case SERD_NTRIPLES:
    return write_ntriples_statement(writer, flags, subject, predicate, object);

  case SERD_NQUADS:
    return write_nquads_statement(
      writer, flags, subject, predicate, object, graph);

  case SERD_TRIG:
    return write_trig_statement(
      writer, flags, subject, predicate, object, graph);
  }

  return SERD_SUCCESS;
}

SERD_NODISCARD static SerdStatus
serd_writer_end_anon(SerdWriter* writer, const SerdNode* node)
{
  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax != SERD_TURTLE && writer->syntax != SERD_TRIG) {
    return SERD_SUCCESS;
  }

  if (!writer->anon_stack_size) {
    return w_err(writer, SERD_BAD_EVENT, "unexpected end of anonymous node\n");
  }

  // Write the end separator ']' and pop the context
  TRY(st, write_sep(writer, writer->context.flags, SEP_ANON_END));
  pop_context(writer);

  if (writer->context.predicate &&
      serd_node_equals(node, writer->context.subject)) {
    // Now-finished anonymous node is the new subject with no other context
    memset(writer->context.predicate, 0, sizeof(SerdNode));
  }

  return st;
}

SERD_NODISCARD static SerdStatus
serd_writer_on_event(SerdWriter* writer, const SerdEvent* event)
{
  switch (event->type) {
  case SERD_BASE:
    return serd_writer_set_base_uri(writer, event->base.uri);
  case SERD_PREFIX:
    return serd_writer_set_prefix(
      writer, event->prefix.name, event->prefix.uri);
  case SERD_STATEMENT:
    return serd_writer_write_statement(
      writer, event->statement.flags, event->statement.statement);
  case SERD_END:
    return serd_writer_end_anon(writer, event->end.node);
  }

  return SERD_BAD_ARG;
}

SerdStatus
serd_writer_finish(SerdWriter* writer)
{
  assert(writer);

  const SerdStatus st0 = terminate_context(writer);
  const SerdStatus st1 = serd_block_dumper_flush(&writer->output);

  free_anon_stack(writer);
  reset_context(writer, RESET_GRAPH | RESET_INDENT);
  writer->last_sep = SEP_NONE;
  return st0 ? st0 : st1;
}

SerdWriter*
serd_writer_new(SerdWorld*        world,
                SerdSyntax        syntax,
                SerdWriterFlags   flags,
                SerdEnv*          env,
                SerdOutputStream* output,
                size_t            block_size)
{
  assert(world);
  assert(env);
  assert(output);

  SerdBlockDumper dumper = {world->allocator, NULL, NULL, 0U, 0U};
  if (serd_block_dumper_open(world, &dumper, output, block_size)) {
    return NULL;
  }

  const WriteContext context = WRITE_CONTEXT_NULL;

  SerdWriter* writer = (SerdWriter*)serd_wcalloc(world, 1, sizeof(SerdWriter));
  if (!writer) {
    serd_block_dumper_close(&dumper);
    return NULL;
  }

  writer->world     = world;
  writer->syntax    = syntax;
  writer->flags     = flags;
  writer->env       = env;
  writer->root_node = NULL;
  writer->root_uri  = SERD_URI_NULL;
  writer->output    = dumper;
  writer->context   = context;

  if (world->limits.writer_max_depth) {
    writer->max_depth  = world->limits.writer_max_depth;
    writer->anon_stack = (WriteContext*)serd_wcalloc(
      world, world->limits.writer_max_depth, sizeof(WriteContext));
    if (!writer->anon_stack) {
      serd_wfree(world, writer);
      return NULL;
    }
  }

  writer->iface.allocator = world->allocator;
  writer->iface.handle    = writer;
  writer->iface.on_event  = (SerdEventFunc)serd_writer_on_event;

  return writer;
}

SERD_NODISCARD static SerdStatus
serd_writer_set_base_uri(SerdWriter* writer, const SerdNode* uri)
{
  assert(writer);

  SERD_DISABLE_NULL_WARNINGS

  if (uri && serd_node_type(uri) != SERD_URI) {
    return SERD_BAD_ARG;
  }

  if (serd_node_equals(serd_env_base_uri(writer->env), uri)) {
    return SERD_SUCCESS;
  }

  const ZixStringView uri_string =
    uri ? serd_node_string_view(uri) : zix_empty_string();

  SerdStatus st = SERD_SUCCESS;
  TRY(st, serd_env_set_base_uri(writer->env, uri_string));

  if (uri && (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG)) {
    TRY(st, terminate_context(writer));
    TRY(st, esink("@base <", 7, writer));
    TRY(st, esink(uri_string.data, uri_string.length, writer));
    TRY(st, esink(">", 1, writer));
    writer->last_sep = SEP_NODE;
    TRY(st, write_sep(writer, writer->context.flags, SEP_END_DIRECT));
  }

  SERD_RESTORE_WARNINGS

  return reset_context(writer, RESET_GRAPH | RESET_INDENT);
}

SerdStatus
serd_writer_set_root_uri(SerdWriter* writer, const ZixStringView uri)
{
  assert(writer);

  serd_node_free(writer->world->allocator, writer->root_node);
  writer->root_node = NULL;
  writer->root_uri  = SERD_URI_NULL;

  if (uri.length) {
    writer->root_node = serd_new_uri(writer->world->allocator, uri);
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
    if (writer->flags & SERD_WRITE_CONTEXTUAL) {
      return st;
    }

    TRY(st, esink("@prefix ", 8, writer));
    TRY(st, esink(serd_node_string(name), name->length, writer));
    TRY(st, esink(": <", 3, writer));
    TRY(st, ewrite_uri(writer, serd_node_string(uri), uri->length));
    TRY(st, esink(">", 1, writer));
    writer->last_sep = SEP_NODE;
    TRY(st, write_sep(writer, writer->context.flags, SEP_END_DIRECT));
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
  free_context(writer);
  free_anon_stack(writer);
  serd_block_dumper_close(&writer->output);
  serd_wfree(writer->world, writer->anon_stack);
  serd_node_free(writer->world->allocator, writer->root_node);
  serd_wfree(writer->world, writer);
}

const SerdSink*
serd_writer_sink(SerdWriter* writer)
{
  assert(writer);
  return &writer->iface;
}
