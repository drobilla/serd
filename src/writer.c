// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_sink.h"
#include "namespaces.h"
#include "stack.h"
#include "string_utils.h"
#include "symbols.h"
#include "system.h"
#include "token_header.h"
#include "try.h"
#include "uri_utils.h"
#include "world_internal.h"

#include <serd/buffer.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/field.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/stream.h>
#include <serd/string_pair_view.h>
#include <serd/syntax.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

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

typedef struct WriteContext {
  struct WriteContext* back;
  ContextType          type;
  bool                 comma_indented;
  TokenHeader          graph;
  TokenHeader          subject;
  TokenHeader          predicate;
} WriteContext;

typedef enum {
  SEP_NONE,     ///< Sentinel after "nothing"
  SEP_STOP,     ///< End of a subject or directive ('.')
  SEP_END_P,    ///< End of a predicate (';')
  SEP_END_O_N,  ///< End of an object before a named one (',')
  SEP_END_O_NA, ///< End of named object before an anonymous one (',')
  SEP_END_O_AA, ///< End of anonymous object before another (',')
  SEP_S_P,      ///< Between a subject and predicate (whitespace)
  SEP_P_O,      ///< Between a predicate and object (whitespace)
  SEP_GRAPH_L,  ///< Start of graph ('{')
  SEP_GRAPH_R,  ///< End of graph ('}')
  SEP_ANON_L,   ///< Start of anonymous node ('[')
  SEP_ANON_R,   ///< End of anonymous node (']')
  SEP_LIST_L,   ///< Start of list ('(')
  SEP_LIST_SEP, ///< List separator (whitespace)
  SEP_LIST_R,   ///< End of list (')')
} Sep;

typedef enum {
  PRE_SPACE  = (1U << 0U), ///< Leading space
  PRE_LINE   = (1U << 1U), ///< Leading newline
  POST_SPACE = (1U << 2U), ///< Trailing space
  POST_LINE  = (1U << 3U), ///< Trailing newline
} SepFlag;

typedef struct {
  char    sep;        ///< Sep character
  int     indent : 4; ///< Indent delta
  uint8_t flags : 4;  ///< Whitespace flags
} SepRule;

#define NIL '\0'

static const SepRule rules[] = {
  {NIL, +0, 0U},
  {'.', +0, PRE_SPACE},
  {';', +0, PRE_SPACE | POST_LINE},
  {',', +0, PRE_SPACE | POST_LINE},
  {',', +0, PRE_SPACE | POST_LINE},
  {',', +0, PRE_SPACE | POST_SPACE},
  {NIL, +1, POST_LINE},
  {' ', +0, 0U},
  {'{', +1, PRE_SPACE | POST_LINE},
  {'}', -1, POST_LINE},
  {'[', +1, 0U},
  {']', -1, PRE_LINE},
  {'(', +1, POST_LINE},
  {NIL, +0, PRE_LINE},
  {')', -1, PRE_LINE},
};

#undef NIL

struct SerdWriterImpl {
  SerdSink        sink;
  SerdWorld*      world;
  SerdSyntax      syntax;
  SerdWriterFlags flags;
  SerdEnv*        env;
  char*           root_uri_string;
  SerdURIView     root_uri;
  SerdStack       stack;
  SerdByteSink    byte_sink;
  WriteContext*   context;
  char*           bprefix;
  size_t          bprefix_len;
  Sep             last_sep;
  int             indent;
};

typedef enum { WRITE_STRING, WRITE_LONG_STRING } TextContext;
typedef enum { RESET_GRAPH = 1U << 0U, RESET_INDENT = 1U << 1U } ResetFlag;

static const uint8_t replacement_char[] = {0xEFU, 0xBFU, 0xBDU};

ZIX_NODISCARD static SerdStatus
serd_writer_on_event(void* handle, const SerdEvent* event);

ZIX_NODISCARD static bool
supports_abbrev(const SerdWriter* const writer)
{
  return writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG;
}

ZIX_LOG_FUNC(3, 4)
static SerdStatus
w_err(SerdWriter* const writer, const SerdStatus st, const char* const fmt, ...)
{
  /* TODO: This results in errors with no file information, which is not
     helpful when re-serializing a file (particularly for "undefined
     namespace prefix" errors.  The statement sink API needs to be changed to
     add a caret parameter so the source can notify the writer of the
     statement origin for better error reporting. */

  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);

  serd_world_verrorf(writer->world, st, fmt, args);

  va_end(args);
  return st;
}

static size_t
pad_stack_size(const size_t size)
{
  return (size + 7U) & ~0x07U;
}

static SerdStatus
top_set(SerdWriter* const   writer,
        const SerdField     field,
        const SerdTokenView src)
{
  assert(field != SERD_OBJECT);

  WriteContext* const ctx = writer->context;
  TokenHeader* const  dst = (field == SERD_SUBJECT)     ? &ctx->subject
                            : (field == SERD_PREDICATE) ? &ctx->predicate
                                                        : &ctx->graph;

  // Pop/reset predicate (the top string) unconditionally
  size_t pop_bytes = ctx->predicate.length + !!ctx->predicate.length;
  memset(&ctx->predicate, 0, sizeof(ctx->predicate));

  if (field == SERD_SUBJECT) {
    // Pop the subject (to be reset below)
    pop_bytes += ctx->subject.length + !!ctx->subject.length;
  } else if (field == SERD_GRAPH) {
    // Pop/reset the subject, and pop the graph (to be reset below)
    pop_bytes += ctx->subject.length + !!ctx->subject.length +
                 ctx->graph.length + !!ctx->graph.length;
    memset(&ctx->subject, 0, sizeof(ctx->subject));
  }

  serd_stack_pop(&writer->stack, pop_bytes);
  dst->type   = src.type;
  dst->flags  = 0U;
  dst->length = (uint32_t)src.string.length;

  if (src.string.length) {
    void* const str = serd_stack_push(&writer->stack, src.string.length + 1U);
    if (!str) {
      return SERD_BAD_STACK;
    }

    memcpy(str, src.string.data, src.string.length);
    ((char*)str)[src.string.length] = '\0';
  }

  return SERD_SUCCESS;
}

static const TokenHeader*
top_get(const SerdWriter* const writer, const SerdField field)
{
  assert(field != SERD_OBJECT);
  return (field == SERD_SUBJECT)     ? &writer->context->subject
         : (field == SERD_PREDICATE) ? &writer->context->predicate
                                     : &writer->context->graph;
}

static bool
top_has_field(const SerdWriter* const writer, const SerdField field)
{
  return top_get(writer, field)->type;
}

static SerdTokenView
top_view(const SerdWriter* const writer, const SerdField field)
{
  const WriteContext* const ctx = writer->context;

  const TokenHeader* const dst = top_get(writer, field);
  if (!dst->type) {
    return serd_no_token();
  }

  size_t start_offset = 0;
  if (field == SERD_SUBJECT) {
    start_offset = ctx->graph.length + !!ctx->graph.length;
  } else if (field == SERD_PREDICATE) {
    start_offset = ctx->graph.length + !!ctx->graph.length +
                   ctx->subject.length + !!ctx->subject.length;
  }

  const char* const str = ((const char*)(ctx + 1)) + start_offset;
  return serd_token_view(dst->type, zix_substring(str, dst->length));
}

static bool
top_is_nested(const SerdWriter* const writer)
{
  return writer->context->back != writer->context;
}

static size_t
append_string(char* const buf, const size_t len, const char* const str)
{
  if (!len) {
    return 0;
  }

  memcpy(buf, str, len);
  buf[len] = '\0';
  return len + 1;
}

ZIX_NODISCARD static SerdStatus
push_context(SerdWriter* const   writer,
             const ContextType   type,
             const SerdTokenView graph,
             const SerdTokenView subject,
             const SerdTokenView predicate)
{
  // Push padding for 64-bit alignment if necessary
  const size_t padded_size = pad_stack_size(writer->stack.size);
  if (padded_size > writer->stack.size) {
    (void)serd_stack_push(&writer->stack, padded_size - writer->stack.size);
  }

  // Push space for the header and terminated strings padded to 64 bits
  const size_t g_size     = graph.string.length + !!graph.string.length;
  const size_t s_size     = subject.string.length + !!subject.string.length;
  const size_t p_size     = predicate.string.length + !!predicate.string.length;
  const size_t sizes      = g_size + s_size + p_size;
  const size_t total_size = sizeof(WriteContext) + sizes;
  void* const  top        = serd_stack_push(&writer->stack, total_size);
  if (!top) {
    return SERD_BAD_STACK;
  }

  // Set header fields
  WriteContext* const ctx = (WriteContext*)top;
  ctx->back               = writer->context;
  ctx->type               = type;
  ctx->comma_indented     = false;
  ctx->graph.type         = graph.type;
  ctx->graph.flags        = 0U;
  ctx->graph.length       = (uint32_t)graph.string.length;
  ctx->subject.type       = subject.type;
  ctx->subject.flags      = 0U;
  ctx->subject.length     = (uint32_t)subject.string.length;
  ctx->predicate.type     = predicate.type;
  ctx->predicate.flags    = 0U;
  ctx->predicate.length   = (uint32_t)predicate.string.length;

  // Append strings, one after the other, separated by null bytes
  char* s = (char*)(ctx + 1);
  s += append_string(s, graph.string.length, graph.string.data);
  s += append_string(s, subject.string.length, subject.string.data);
  append_string(s, predicate.string.length, predicate.string.data);

  writer->context = ctx;
  return SERD_SUCCESS;
}

static void
pop_context(SerdWriter* const writer)
{
  WriteContext* const top = writer->context;

  if (top->back != top) {
    writer->context = top->back;

    WriteContext* const ctx = writer->context;

    const size_t strings_size = ctx->graph.length + !!ctx->graph.length +
                                ctx->subject.length + !!ctx->subject.length +
                                ctx->predicate.length + !!ctx->predicate.length;

    const char* const new_top = ((const char*)(ctx + 1)) + strings_size;
    serd_stack_pop_to(&writer->stack, (size_t)(new_top - writer->stack.buf));
  }
}

ZIX_NODISCARD static size_t
sink(const void* const buf, const size_t len, SerdWriter* const writer)
{
  const size_t written = serd_byte_sink_write(buf, len, &writer->byte_sink);
  if (written != len) {
    if (errno) {
      w_err(writer, SERD_BAD_WRITE, "write error (%s)", strerror(errno));
    } else {
      w_err(writer, SERD_BAD_WRITE, "write error");
    }
  }

  return written;
}

ZIX_NODISCARD static SerdStatus
esink(const void* const buf, const size_t len, SerdWriter* const writer)
{
  return sink(buf, len, writer) == len ? SERD_SUCCESS : SERD_BAD_WRITE;
}

// Write a single character, as an escape for single byte characters
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
    *st = w_err(writer, SERD_BAD_TEXT, "invalid UTF-8 start: %X", utf8[0]);
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
  return (c == '"') || (c == '<') || (c == '>') || (c == '\\') || (c == '^') ||
         (c == '`') || in_range(c, '{', '}') || !in_range(c, 0x21, 0x7E);
}

static size_t
write_uri_text(SerdWriter* const writer,
               const char* const utf8,
               const size_t      n_bytes,
               SerdStatus* const st)
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
    uint8_t size = 0U;
    len += write_character(writer, (const uint8_t*)utf8 + i, &size, st);
    i += size;
    if (*st && !(writer->flags & SERD_WRITE_LAX)) {
      break;
    }

    if (!size) {
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

ZIX_NODISCARD static SerdStatus
ewrite_uri(SerdWriter* const writer, const ZixStringView string)
{
  SerdStatus st = SERD_SUCCESS;
  write_uri_text(writer, string.data, string.length, &st);

  return (st == SERD_BAD_WRITE || !(writer->flags & SERD_WRITE_LAX))
           ? st
           : SERD_SUCCESS;
}

static bool
lname_must_escape(const char c)
{
  /* Most of these characters have nothing to do with Turtle, but were taken
     from SPARQL and mashed into the Turtle grammar (despite not being used)
     with RDF 1.1.  So now Turtle is a mess because the SPARQL grammar is
     poorly designed and didn't use a leading character to distinguish things
     like path patterns like it should have.

     Note that '-', '.', and '_' are also in PN_LOCAL_ESC, but are valid
     unescaped in local names, so they are not escaped here. */

  return (c == '!') || (c == '/') || (c == ';') || (c == '=') || (c == '?') ||
         (c == '@') || (c == '~') || in_range(c, '#', ',');
}

ZIX_NODISCARD static SerdStatus
write_lname(SerdWriter* const writer, const ZixStringView string)
{
  const char* const utf8    = string.data;
  const size_t      n_bytes = string.length;
  SerdStatus        st      = SERD_SUCCESS;

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

ZIX_NODISCARD static SerdStatus
write_text(SerdWriter* const writer,
           const TextContext ctx,
           const char* const utf8,
           const size_t      n_bytes)
{
  assert(utf8);

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
        if (n_consecutive_quotes >= 3) {
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
    uint8_t size = 0U;
    write_character(writer, (const uint8_t*)utf8 + i - 1, &size, &st);
    if (st && !(writer->flags & SERD_WRITE_LAX)) {
      return st;
    }

    if (size > 0U) {
      i += size - 1U;
    } else {
      // Corrupt input, write replacement character and scan to the next start
      st = esink(replacement_char, sizeof(replacement_char), writer);
      for (; i < n_bytes && !is_utf8_leading((uint8_t)utf8[i]); ++i) {
      }
    }
  }

  return (writer->flags & SERD_WRITE_LAX) ? SERD_SUCCESS : st;
}

typedef struct {
  SerdWriter* writer;
  SerdStatus  status;
} UriSinkContext;

ZIX_NODISCARD static size_t
uri_sink(const void* const buf, const size_t len, void* const stream)
{
  UriSinkContext* const context = (UriSinkContext*)stream;
  SerdWriter* const     writer  = context->writer;

  return write_uri_text(writer, (const char*)buf, len, &context->status);
}

ZIX_NODISCARD static SerdStatus
write_newline(SerdWriter* const writer)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, esink("\n", 1, writer));
  for (int i = 0; i < writer->indent; ++i) {
    TRY(st, esink("\t", 1, writer));
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_space(SerdWriter* const writer, const uint8_t flags)
{
  return (flags & PRE_LINE)    ? write_newline(writer)
         : (flags & PRE_SPACE) ? esink(" ", 1, writer)
                               : SERD_SUCCESS;
}

ZIX_NODISCARD static SerdStatus
write_sep(SerdWriter* const writer, const Sep sep)
{
  SerdStatus           st   = SERD_SUCCESS;
  const SepRule* const rule = &rules[sep];
  WriteContext* const  ctx  = writer->context;

  // Adjust indent, but tolerate if it would become negative
  if (rule->indent && (rule->flags & (PRE_LINE | POST_LINE))) {
    writer->indent += rule->indent;
  }

  // Adjust indentation for object comma if necessary
  if (sep == SEP_END_O_N && !ctx->comma_indented) {
    ++writer->indent;
    ctx->comma_indented = true;
  } else if (sep == SEP_END_P && ctx->comma_indented) {
    --writer->indent;
    ctx->comma_indented = false;
  }

  // Write newline or space before separator if necessary
  TRY(st, write_space(writer, rule->flags));

  // Write actual separator string
  if (rule->sep) {
    TRY(st, esink(&rule->sep, 1, writer));
  }

  // Write newline after separator if necessary
  TRY(st, write_space(writer, rule->flags >> 2U));

  // Reset context and write a blank line after ends of subjects
  if (sep == SEP_STOP) {
    writer->indent      = top_has_field(writer, SERD_GRAPH) ? 1 : 0;
    ctx->comma_indented = false;
    TRY(st, esink("\n", 1, writer));
  }

  writer->last_sep = sep;
  return st;
}

static SerdStatus
reset_context(SerdWriter* const writer, const unsigned flags)
{
  WriteContext* const ctx = writer->context;

  if (flags & RESET_GRAPH) {
    ctx->graph.type = SERD_NOTHING;
  }

  if (flags & RESET_INDENT) {
    writer->indent = 0;
  }

  ctx->type           = CTX_NAMED;
  ctx->subject.type   = SERD_NOTHING;
  ctx->predicate.type = SERD_NOTHING;
  ctx->comma_indented = false;
  return SERD_SUCCESS;
}

// Return the name of the XSD datatype referred to by `datatype`, if any
static const char*
get_xsd_name(const SerdEnv* const env, const SerdTokenView datatype)
{
  if (datatype.type == SERD_URI &&
      (!strncmp(datatype.string.data, NS_XSD, sizeof(NS_XSD) - 1))) {
    return datatype.string.data + sizeof(NS_XSD) - 1U;
  }

  if (datatype.type == SERD_CURIE) {
    // We can be a bit lazy/presumptive here due to grammar limitations
    SerdStringPairView pair = {{"", 0}, {"", 0}};
    if (!serd_env_expand(env, datatype.string, &pair)) {
      if (!strcmp(pair.prefix.data, NS_XSD)) {
        return pair.suffix.data;
      }
    }
  }

  return "";
}

static bool
token_equals_symbol(const SerdTokenView token, const SerdSymbol symbol)
{
  return zix_string_view_equals(token.string, serd_symbols[symbol]);
}

// Return true iff `buf` is a valid prefixed name prefix or suffix
static bool
is_name(const char* const buf, const size_t len)
{
  // TODO: This is more strict than it should be
  for (size_t i = 0; i < len; ++i) {
    if (!(is_alpha(buf[i]) || is_digit(buf[i]))) {
      return false;
    }
  }

  return true;
}

ZIX_NODISCARD static SerdStatus
write_uri(SerdWriter* const writer, const ZixStringView string)
{
  SerdStatus st         = SERD_SUCCESS;
  const bool has_scheme = serd_uri_string_has_scheme(string.data);

  if (supports_abbrev(writer)) {
    if (token_equals_symbol(serd_token_view(SERD_URI, string), RDF_NIL)) {
      return esink("()", 2, writer);
    }

    SerdStringPairView pair = {{"", 0}, {"", 0}};
    if (has_scheme && !(writer->flags & SERD_WRITE_UNQUALIFIED) &&
        !serd_env_qualify(writer->env, string, &pair) &&
        is_name(pair.prefix.data, pair.prefix.length) &&
        is_name(pair.suffix.data, pair.suffix.length)) {
      TRY(st, write_lname(writer, pair.prefix));
      TRY(st, esink(":", 1, writer));
      return ewrite_uri(writer, pair.suffix);
    }
  }

  if (!has_scheme &&
      (writer->syntax == SERD_NTRIPLES || writer->syntax == SERD_NQUADS) &&
      !serd_uri_has_scheme(serd_env_base_uri_view(writer->env))) {
    return w_err(writer,
                 SERD_BAD_ARG,
                 "unable to resolve <%s> without a base URI",
                 string.data);
  }

  TRY(st, esink("<", 1, writer));

  if (!(writer->flags & SERD_WRITE_UNRESOLVED) &&
      serd_env_base_uri_string(writer->env).length) {
    const SerdURIView  base_uri = serd_env_base_uri_view(writer->env);
    const SerdURIView  uri      = serd_parse_uri(string.data);
    const SerdURIView  abs_uri  = serd_resolve_uri(uri, base_uri);
    const bool         rooted   = uri_is_under(&base_uri, &writer->root_uri);
    const SerdURIView* root     = rooted ? &writer->root_uri : &base_uri;
    UriSinkContext     context  = {writer, SERD_SUCCESS};

    if (!supports_abbrev(writer) || !uri_is_under(&abs_uri, root)) {
      serd_write_uri(abs_uri, uri_sink, &context);
    } else {
      serd_write_uri(serd_relative_uri(uri, base_uri), uri_sink, &context);
    }
  } else {
    st = ewrite_uri(writer, string);
  }

  return st ? st : esink(">", 1, writer);
}

ZIX_NODISCARD static SerdStatus
write_curie(SerdWriter* const writer, const ZixStringView curie)
{
  SerdStringPairView pair = {{"", 0}, {"", 0}};
  SerdStatus         st   = SERD_SUCCESS;

  // In fast-and-loose Turtle/TriG mode CURIEs are simply passed through
  const bool fast =
    (writer->flags & (SERD_WRITE_UNQUALIFIED | SERD_WRITE_UNRESOLVED));

  if (!supports_abbrev(writer) || !fast) {
    if ((st = serd_env_expand(writer->env, curie, &pair))) {
      return w_err(writer, st, "undefined namespace prefix '%s'", curie.data);
    }
  }

  if (!supports_abbrev(writer)) {
    TRY(st, esink("<", 1, writer));
    TRY(st, ewrite_uri(writer, pair.prefix));
    TRY(st, ewrite_uri(writer, pair.suffix));
    TRY(st, esink(">", 1, writer));
  } else {
    TRY(st, write_lname(writer, curie));
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_iri(SerdWriter* const writer, const SerdTokenView node)
{
  return (node.type == SERD_URI)     ? write_uri(writer, node.string)
         : (node.type == SERD_CURIE) ? write_curie(writer, node.string)
                                     : SERD_BAD_ARG;
}

ZIX_NODISCARD static SerdStatus
write_literal(SerdWriter* const   writer,
              const ZixStringView string,
              const SerdNodeFlags node_flags,
              const SerdTokenView meta)
{
  SerdStatus st = SERD_SUCCESS;

  if (supports_abbrev(writer) && (node_flags & SERD_HAS_DATATYPE)) {
    const char* const xsd_name = get_xsd_name(writer->env, meta);
    if (!strcmp(xsd_name, "boolean") || !strcmp(xsd_name, "integer") ||
        (!strcmp(xsd_name, "decimal") && strchr(string.data, '.') &&
         string.data[string.length - 1U] != '.')) {
      return esink(string.data, string.length, writer);
    }
  }

  if (supports_abbrev(writer) &&
      (node_flags & (SERD_HAS_NEWLINE | SERD_HAS_QUOTE))) {
    TRY(st, esink("\"\"\"", 3, writer));
    TRY(st, write_text(writer, WRITE_LONG_STRING, string.data, string.length));
    st = esink("\"\"\"", 3, writer);
  } else {
    TRY(st, esink("\"", 1, writer));
    TRY(st, write_text(writer, WRITE_STRING, string.data, string.length));
    st = esink("\"", 1, writer);
  }

  if (node_flags & SERD_HAS_LANGUAGE) {
    TRY(st, esink("@", 1, writer));
    st = esink(meta.string.data, meta.string.length, writer);
  } else if (node_flags & SERD_HAS_DATATYPE) {
    TRY(st, esink("^^", 2, writer));
    st = write_iri(writer, meta);
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_blank(SerdWriter* const    writer,
            const ZixStringView  label,
            const SerdField      field,
            const SerdEventFlags flags)
{
  SerdStatus st = SERD_SUCCESS;

  if (supports_abbrev(writer)) {
    if ((field == SERD_SUBJECT && (flags & SERD_ANON_S)) ||
        (field == SERD_OBJECT && (flags & SERD_ANON_O))) {
      return write_sep(writer, SEP_ANON_L);
    }

    if ((field == SERD_SUBJECT && (flags & SERD_LIST_S)) ||
        (field == SERD_OBJECT && (flags & SERD_LIST_O))) {
      return write_sep(writer, SEP_LIST_L);
    }

    if ((field == SERD_SUBJECT && (flags & SERD_EMPTY_S)) ||
        (field == SERD_OBJECT && (flags & SERD_EMPTY_O))) {
      return esink("[]", 2, writer);
    }
  }

  TRY(st, esink("_:", 2, writer));
  if (writer->bprefix &&
      !strncmp(label.data, writer->bprefix, writer->bprefix_len)) {
    TRY(st,
        esink(label.data + writer->bprefix_len,
              label.length - writer->bprefix_len,
              writer));
  } else {
    TRY(st, esink(label.data, label.length, writer));
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_token(SerdWriter* const    writer,
            const SerdField      field,
            const SerdEventFlags event_flags,
            const SerdTokenView  node)
{
  return (node.type == SERD_URI)     ? write_uri(writer, node.string)
         : (node.type == SERD_CURIE) ? write_curie(writer, node.string)
         : (node.type == SERD_BLANK)
           ? write_blank(writer, node.string, field, event_flags)
           : SERD_BAD_ARG;
}

ZIX_NODISCARD static SerdStatus
write_object(SerdWriter* const    writer,
             const SerdEventFlags event_flags,
             const SerdObjectView node)
{
  return (node.type == SERD_LITERAL)
           ? write_literal(writer, node.string, node.flags, node.meta)
           : write_token(
               writer, SERD_OBJECT, event_flags, serd_object_token_view(node));
}

ZIX_NODISCARD static SerdStatus
write_pred(SerdWriter* writer, const SerdTokenView pred)
{
  SerdStatus st = token_equals_symbol(pred, RDF_TYPE) ? esink("a", 1, writer)
                                                      : write_iri(writer, pred);

  if (!st) {
    st = write_sep(writer, SEP_P_O);
  }

  return st ? st : top_set(writer, SERD_PREDICATE, pred);
}

ZIX_NODISCARD static SerdStatus
write_list_next(SerdWriter* const    writer,
                const SerdEventFlags flags,
                const SerdTokenView  predicate,
                const SerdObjectView object)
{
  SerdStatus st = SERD_SUCCESS;

  if (token_equals_symbol(serd_object_token_view(object), RDF_NIL)) {
    TRY(st, write_sep(writer, SEP_LIST_R));
    return SERD_FAILURE;
  }

  if (token_equals_symbol(predicate, RDF_FIRST)) {
    TRY(st, write_object(writer, flags, object));
  } else {
    TRY(st, write_sep(writer, SEP_LIST_SEP));
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
terminate_context(SerdWriter* const writer)
{
  SerdStatus st = SERD_SUCCESS;

  if (top_has_field(writer, SERD_SUBJECT)) {
    TRY(st, write_sep(writer, SEP_STOP));
  }

  if (top_has_field(writer, SERD_GRAPH)) {
    TRY(st, write_sep(writer, SEP_GRAPH_R));
  }

  return st;
}

static bool
top_field_equals(const SerdWriter* const writer,
                 const SerdField         field,
                 const SerdTokenView     token)
{
  assert(field != SERD_OBJECT);
  const SerdTokenView view = top_view(writer, field);
  return token.type == view.type &&
         zix_string_view_equals(view.string, token.string);
}

ZIX_NODISCARD static SerdStatus
write_statement(SerdWriter* const       writer,
                const SerdEventFlags    flags,
                const SerdStatementView statement)
{
  assert(writer);

  const WriteContext* const ctx = writer->context;
  SerdStatus                st  = SERD_SUCCESS;

  if (writer->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_SUCCESS;
  }

  const SerdTokenView  subject   = statement.subject;
  const SerdTokenView  predicate = statement.predicate;
  const SerdObjectView object    = statement.object;
  SerdTokenView        graph     = statement.graph;

  // Refuse to write incoherent statements
  if (!serd_field_supports(SERD_SUBJECT, subject.type) ||
      !serd_field_supports(SERD_PREDICATE, predicate.type) ||
      object.type == SERD_NOTHING ||
      ((object.flags & SERD_HAS_DATATYPE) &&
       (object.flags & SERD_HAS_LANGUAGE)) ||
      ((flags & SERD_ANON_S) && (flags & SERD_LIST_S)) ||
      ((flags & SERD_EMPTY_S) && (flags & SERD_LIST_S)) ||
      ((flags & SERD_ANON_O) && (flags & SERD_LIST_O)) ||
      ((flags & SERD_EMPTY_O) && (flags & SERD_LIST_O))) {
    return SERD_BAD_ARG;
  }

  // Simple case: write a line of NTriples or NQuads
  if (writer->syntax == SERD_NTRIPLES || writer->syntax == SERD_NQUADS) {
    TRY(st, write_token(writer, SERD_SUBJECT, flags, subject));
    TRY(st, esink(" ", 1, writer));
    TRY(st, write_token(writer, SERD_PREDICATE, flags, predicate));
    TRY(st, esink(" ", 1, writer));
    TRY(st, write_object(writer, flags, object));
    if (writer->syntax == SERD_NQUADS &&
        serd_field_supports(SERD_GRAPH, graph.type)) {
      TRY(st, esink(" ", 1, writer));
      TRY(st, write_token(writer, SERD_GRAPH, flags, graph));
    }
    TRY(st, esink(" .\n", 3, writer));
    return SERD_SUCCESS;
  }

  if (writer->syntax == SERD_TURTLE) {
    graph = serd_no_token();
  }

  // Separate graphs if necessary
  const bool has_graph = serd_field_supports(SERD_GRAPH, graph.type);
  if ((has_graph && !top_field_equals(writer, SERD_GRAPH, graph)) ||
      (!has_graph && top_has_field(writer, SERD_GRAPH))) {
    TRY(st, terminate_context(writer));
    reset_context(writer, RESET_GRAPH | RESET_INDENT);
    TRY(st, write_newline(writer));
    if (has_graph) {
      TRY(st, write_token(writer, SERD_GRAPH, flags, graph));
      TRY(st, write_sep(writer, SEP_GRAPH_L));
      TRY(st, top_set(writer, SERD_GRAPH, graph));
    }
  }

  if (ctx->type == CTX_LIST) {
    // Continue a list
    if (token_equals_symbol(predicate, RDF_FIRST) &&
        token_equals_symbol(serd_object_token_view(object), RDF_NIL)) {
      return esink("()", 2, writer);
    }

    st = write_list_next(writer, flags, predicate, object);
    if (st == SERD_FAILURE) { // Reached end of list
      pop_context(writer);
      return SERD_SUCCESS;
    }

  } else if (top_field_equals(writer, SERD_SUBJECT, subject)) {
    if (top_field_equals(writer, SERD_PREDICATE, predicate)) {
      // Elide S P (write O)

      const Sep  last        = writer->last_sep;
      const bool anon_o      = flags & SERD_ANON_O;
      const bool list_o      = flags & SERD_LIST_O;
      const bool before_name = !anon_o && !list_o;
      const bool after_end   = (last == SEP_ANON_R) || (last == SEP_LIST_R);

      TRY(st,
          write_sep(writer,
                    before_name ? SEP_END_O_N
                    : after_end ? SEP_END_O_AA
                                : SEP_END_O_NA));

    } else {
      // Elide S (write P and O)

      const bool first = !top_has_field(writer, SERD_PREDICATE);
      TRY(st, write_sep(writer, first ? SEP_S_P : SEP_END_P));
      TRY(st, write_pred(writer, predicate));
    }

    TRY(st, write_object(writer, flags, object));

  } else {
    // No abbreviation

    if (top_is_nested(writer)) {
      return SERD_BAD_ARG;
    }

    if (top_has_field(writer, SERD_SUBJECT)) {
      TRY(st, write_sep(writer, SEP_STOP));
    }

    if (writer->last_sep == SEP_STOP) {
      TRY(st, write_newline(writer));
    }

    // Write subject node
    TRY(st, write_token(writer, SERD_SUBJECT, flags, subject));
    if (!(flags & SERD_LIST_S)) {
      TRY(st, write_sep(writer, SEP_S_P));
    }

    // Set context to new subject
    reset_context(writer, 0U);
    TRY(st, top_set(writer, SERD_SUBJECT, subject));

    // Write predicate
    if (!(flags & SERD_LIST_S)) {
      TRY(st, write_pred(writer, predicate));
    }

    TRY(st, write_object(writer, flags, object));
  }

  if (flags & (SERD_ANON_S | SERD_LIST_S)) {
    // Push context for anonymous or list subject
    const bool is_list = (flags & SERD_LIST_S);

    st = push_context(writer,
                      is_list ? CTX_LIST : CTX_BLANK,
                      graph,
                      subject,
                      is_list ? serd_no_token() : predicate);
  }

  if (flags & (SERD_ANON_O | SERD_LIST_O)) {
    // Push context for anonymous or list object if necessary
    const SerdTokenView anon_subject = {object.type, object.string};

    st = push_context(writer,
                      (flags & SERD_LIST_O) ? CTX_LIST : CTX_BLANK,
                      graph,
                      anon_subject,
                      serd_no_token());
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_end(SerdWriter* const writer, const ZixStringView label)
{
  assert(writer);

  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax != SERD_TURTLE && writer->syntax != SERD_TRIG) {
    return SERD_SUCCESS;
  }

  if (!top_is_nested(writer)) {
    return w_err(writer, SERD_BAD_EVENT, "unexpected end of anonymous node");
  }

  // Decrease indent if we're current comma-indented (multiple objects at end)
  if (writer->context->comma_indented) {
    assert(writer->indent);
    --writer->indent;
    writer->context->comma_indented = false;
  }

  // Write the end separator ']' and pop the context
  TRY(st, write_sep(writer, SEP_ANON_R));
  pop_context(writer);

  const SerdTokenView node = {SERD_BLANK, label};
  if (top_has_field(writer, SERD_PREDICATE) &&
      top_field_equals(writer, SERD_SUBJECT, node)) {
    // Now-finished anonymous node is the new subject with no other context
    writer->context->predicate.type = SERD_NOTHING;
  }

  return st;
}

SerdStatus
serd_writer_finish(SerdWriter* const writer)
{
  assert(writer);

  const SerdStatus st0 = terminate_context(writer);
  const SerdStatus st1 = serd_byte_sink_flush(&writer->byte_sink);
  reset_context(writer, RESET_GRAPH | RESET_INDENT);
  return st0 ? st0 : st1;
}

SerdWriter*
serd_writer_new(SerdWorld* const      world,
                const SerdSyntax      syntax,
                const SerdWriterFlags flags,
                SerdEnv* const        env,
                SerdWriteFunc         ssink,
                void* const           stream)
{
  assert(world);
  assert(env);
  assert(ssink);

  ZixAllocator* const allocator = serd_world_allocator(world);
  const SerdLimits    limits    = serd_world_limits(world);
  if (limits.writer_stack_size < sizeof(WriteContext) + 32) {
    return NULL;
  }

  SerdWriter* writer =
    (SerdWriter*)zix_calloc(allocator, 1, sizeof(SerdWriter));
  if (!writer) {
    return NULL;
  }

  writer->sink.handle   = writer;
  writer->sink.on_event = serd_writer_on_event;

  writer->world  = world;
  writer->syntax = syntax;
  writer->flags  = flags;
  writer->env    = env;

  writer->root_uri_string = NULL;
  writer->root_uri        = serd_empty_uri();

  writer->stack = serd_stack_new(allocator, limits.writer_stack_size);
  if (!writer->stack.buf) {
    zix_free(allocator, writer);
    return NULL;
  }

  void* const ctx = serd_stack_push(&writer->stack, sizeof(WriteContext));
  if (ctx) {
    writer->context       = (WriteContext*)ctx;
    writer->context->back = writer->context;
  }

  writer->byte_sink = serd_byte_sink_new(
    allocator, ssink, stream, (flags & SERD_WRITE_BULK) ? SERD_PAGE_SIZE : 1);
  if (((flags & SERD_WRITE_BULK) && !writer->byte_sink.buf)) {
    serd_stack_free(allocator, &writer->stack);
    zix_free(allocator, writer);
    return NULL;
  }

  return writer;
}

void
serd_writer_chop_blank_prefix(SerdWriter* const writer,
                              const char* const prefix)
{
  assert(writer);

  ZixAllocator* const allocator = serd_world_allocator(writer->world);

  zix_free(allocator, writer->bprefix);
  writer->bprefix_len = 0;
  writer->bprefix     = NULL;

  const size_t prefix_len = prefix ? strlen(prefix) : 0;
  if (prefix_len) {
    writer->bprefix_len = prefix_len;
    writer->bprefix     = (char*)zix_malloc(allocator, writer->bprefix_len + 1);
    memcpy(writer->bprefix, prefix, writer->bprefix_len + 1);
  }
}

ZIX_NODISCARD static SerdStatus
write_base(SerdWriter* const writer, const ZixStringView uri)
{
  if (zix_string_view_equals(serd_env_base_uri_string(writer->env), uri)) {
    return SERD_SUCCESS;
  }

  SerdStatus st = serd_env_set_base_uri(writer->env, uri);
  if (st) {
    return st == SERD_NO_CHANGE ? SERD_SUCCESS : st;
  }

  if (uri.length &&
      (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG)) {
    const bool had_subject = writer->context->subject.type;
    TRY(st, terminate_context(writer));
    if (had_subject) {
      TRY(st, esink("\n", 1, writer));
    }

    TRY(st, esink("@base <", 7, writer));
    TRY(st, esink(uri.data, uri.length, writer));
    TRY(st, esink(">", 1, writer));
    TRY(st, write_sep(writer, SEP_STOP));
  }

  return reset_context(writer, RESET_GRAPH | RESET_INDENT);
}

SerdStatus
serd_writer_set_root_uri(SerdWriter* const writer, const ZixStringView uri)
{
  assert(writer);

  ZixAllocator* const allocator = serd_world_allocator(writer->world);

  zix_free(allocator, writer->root_uri_string);
  writer->root_uri_string = NULL;
  writer->root_uri        = serd_empty_uri();

  if (uri.length) {
    writer->root_uri_string = zix_string_view_copy(allocator, uri);
    writer->root_uri        = serd_parse_uri(writer->root_uri_string);
  }

  return SERD_SUCCESS;
}

ZIX_NODISCARD static SerdStatus
write_prefix(SerdWriter* const   writer,
             const ZixStringView name,
             const ZixStringView uri)
{
  assert(writer);

  SerdStatus st = serd_env_set_prefix(writer->env, name, uri);
  if (st) {
    return st == SERD_NO_CHANGE ? SERD_SUCCESS : st;
  }

  if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
    const bool had_subject = writer->context->subject.type;
    TRY(st, terminate_context(writer));
    if (had_subject) {
      TRY(st, esink("\n", 1, writer));
    }

    TRY(st, esink("@prefix ", 8, writer));
    TRY(st, esink(name.data, name.length, writer));
    TRY(st, esink(": <", 3, writer));
    TRY(st, ewrite_uri(writer, uri));
    TRY(st, esink(">", 1, writer));
    TRY(st, write_sep(writer, SEP_STOP));
  }

  return reset_context(writer, RESET_GRAPH | RESET_INDENT);
}

void
serd_writer_free(SerdWriter* const writer)
{
  if (!writer) {
    return;
  }

  ZixAllocator* const allocator = serd_world_allocator(writer->world);
  serd_writer_finish(writer);
  serd_stack_free(allocator, &writer->stack);
  zix_free(allocator, writer->bprefix);
  serd_byte_sink_free(allocator, &writer->byte_sink);
  zix_free(allocator, writer->root_uri_string);
  zix_free(allocator, writer);
}

const SerdSink*
serd_writer_sink(SerdWriter* const writer)
{
  assert(writer);
  return &writer->sink;
}

size_t
serd_file_sink(const void* const buf, const size_t len, void* const stream)
{
  assert(buf);
  assert(stream);
  return fwrite(buf, 1, len, (FILE*)stream);
}

size_t
serd_buffer_sink(const void* const buf, const size_t len, void* const stream)
{
  assert(buf);
  assert(stream);

  SerdBuffer* buffer = (SerdBuffer*)stream;
  char* const new_buf =
    (char*)zix_realloc(buffer->allocator, buffer->buf, buffer->len + len);

  if (!new_buf) {
    return 0U;
  }

  memcpy(new_buf + buffer->len, buf, len);
  buffer->buf = new_buf;
  buffer->len += len;
  return len;
}

char*
serd_buffer_sink_finish(SerdBuffer* const stream)
{
  assert(stream);
  if (serd_buffer_sink("", 1, stream) < 1U) {
    return NULL;
  }

  return stream->buf;
}

ZIX_NODISCARD static SerdStatus
serd_writer_on_event(void* const handle, const SerdEvent* const event)
{
  SerdWriter* const writer = (SerdWriter*)handle;
  assert(writer);

  SerdStatus st = SERD_BAD_ARG;

  const SerdEventType type = event->type;
  switch (type) {
  case SERD_EVENT_BASE:
    st = write_base(writer, event->body.uri);
    break;
  case SERD_EVENT_PREFIX:
    st = write_prefix(
      writer, event->body.prefix.prefix, event->body.prefix.suffix);
    break;
  case SERD_EVENT_STATEMENT:
    st = write_statement(writer, event->flags, event->body.statement);
    break;
  case SERD_EVENT_END:
    st = write_end(writer, event->body.label);
    break;
  }

  return st;
}
