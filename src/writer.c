// Copyright 2011-2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_sink.h"
#include "namespaces.h"
#include "stack.h"
#include "string_utils.h"
#include "symbols.h"
#include "system.h"
#include "try.h"
#include "turtle.h"
#include "uri_utils.h"
#include "world_internal.h"

#include <serd/buffer.h>
#include <serd/env.h>
#include <serd/field.h>
#include <serd/node.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/statement_flags.h>
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

typedef struct {
  ContextType type;
  bool        comma_indented;
  SerdNode    graph;
  SerdNode    subject;
  SerdNode    predicate;
} WriteContext;

/// A status for an operation that reads/writes variable numbers of bytes
typedef struct {
  SerdStatus status;
  size_t     read_count;
  size_t     write_count;
} VariableResult;

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
  int8_t  indent : 4; ///< Indent delta
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
  SerdWorld*      world;
  SerdSyntax      syntax;
  SerdWriterFlags flags;
  SerdEnv*        env;
  char*           root_uri_string;
  SerdURIView     root_uri;
  SerdStack       anon_stack;
  SerdByteSink    byte_sink;
  WriteContext    context;
  char*           bprefix;
  size_t          bprefix_len;
  Sep             last_sep;
  int             indent;
};

typedef enum { WRITE_STRING, WRITE_LONG_STRING } TextContext;
typedef enum { RESET_GRAPH = 1U << 0U, RESET_INDENT = 1U << 1U } ResetFlag;

typedef bool (*BytePredicate)(uint8_t) ZIX_NODISCARD;

static const uint8_t replacement_char[] = {0xEFU, 0xBFU, 0xBDU};

ZIX_NODISCARD static bool
supports_abbrev(const SerdWriter* const writer)
{
  return writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG;
}

static SerdStatus
free_context(SerdWriter* const writer)
{
  ZixAllocator* const allocator = serd_world_allocator(writer->world);
  serd_node_free(allocator, &writer->context.graph);
  serd_node_free(allocator, &writer->context.subject);
  serd_node_free(allocator, &writer->context.predicate);
  writer->context.graph.type     = SERD_NOTHING;
  writer->context.subject.type   = SERD_NOTHING;
  writer->context.predicate.type = SERD_NOTHING;
  return SERD_SUCCESS;
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

static void
set_current(ZixAllocator* const allocator,
            SerdNode* const     dst,
            const SerdTokenView src)
{
  const size_t new_size = src.string.length + 1U;
  char* const  new_buf =
    (char*)zix_realloc(allocator, (char*)dst->buf, new_size);
  if (new_buf) {
    dst->buf     = new_buf;
    dst->n_bytes = src.string.length;
    dst->flags   = 0U;
    dst->type    = src.type;
    memcpy(new_buf, src.string.data, src.string.length);
    new_buf[src.string.length] = '\0';
  }
}

static const SerdNode*
current_field(const SerdWriter* const writer, const SerdField field)
{
  return (field == SERD_SUBJECT)     ? &writer->context.subject
         : (field == SERD_PREDICATE) ? &writer->context.predicate
         : (field == SERD_GRAPH)     ? &writer->context.graph
                                     : NULL;
}

static inline bool
is_set(const SerdWriter* const writer, const SerdField field)
{
  const SerdNode* const node = current_field(writer, field);
  return node && node->type;
}

static bool
current_field_equals(const SerdWriter* const writer,
                     const SerdField         field,
                     const SerdTokenView     token)
{
  assert(field != SERD_OBJECT);
  const SerdNode* const current = current_field(writer, field);
  return current && current->type == token.type &&
         zix_string_view_equals(serd_node_string_view(current), token.string);
}

static SerdNode
new_token(ZixAllocator* const allocator,
          const SerdNodeType  type,
          const ZixStringView string)
{
  char* const buf = (char*)zix_calloc(allocator, string.length + 1U, 1U);
  if (buf) {
    memcpy(buf, string.data, string.length);
    buf[string.length] = '\0';
  }

  const SerdNode node = {buf, string.length, 0U, type};
  return node;
}

static void
push_context(SerdWriter* const   writer,
             const ContextType   type,
             const SerdTokenView graph,
             const SerdTokenView subject,
             const SerdTokenView predicate)
{
  ZixAllocator* const allocator = serd_world_allocator(writer->world);

  // Push the current context to the stack
  void* const top = serd_stack_push(&writer->anon_stack, sizeof(WriteContext));
  *(WriteContext*)top = writer->context;

  // Update the current context

  const WriteContext current = {
    type,
    false,

    serd_field_supports(SERD_GRAPH, graph.type)
      ? new_token(allocator, graph.type, graph.string)
      : SERD_NODE_NULL,

    new_token(allocator, subject.type, subject.string),

    serd_field_supports(SERD_PREDICATE, predicate.type)
      ? new_token(allocator, predicate.type, predicate.string)
      : SERD_NODE_NULL,
  };

  writer->context = current;
}

static void
pop_context(SerdWriter* const writer)
{
  // Replace the current context with the top of the stack
  free_context(writer);
  writer->context =
    *(WriteContext*)(writer->anon_stack.buf + writer->anon_stack.size -
                     sizeof(WriteContext));

  // Pop the top of the stack away
  serd_stack_pop(&writer->anon_stack, sizeof(WriteContext));
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

/// Variable sink that returns an updated variable result
ZIX_NODISCARD static VariableResult
vsink(SerdWriter* const    writer,
      const VariableResult vr,
      const size_t         len,
      const void* const    buf)
{
  const size_t         n  = sink(buf, len, writer);
  const SerdStatus     st = (n == len) ? SERD_SUCCESS : SERD_BAD_WRITE;
  const VariableResult r  = {st, vr.read_count, vr.write_count + n};
  return r;
}

ZIX_NODISCARD static SerdStatus
write_hex_byte(SerdWriter* const writer, const unsigned byte)
{
  static const char hex_chars[] = "0123456789ABCDEF";

  const char digits[2] = {hex_chars[byte >> 4U], hex_chars[byte & 0x0FU]};

  return esink(digits, 2, writer);
}

static VariableResult
write_UCHAR(SerdWriter* const writer, const uint8_t* const utf8)
{
  uint8_t        c_size = 0U;
  const uint32_t c      = parse_utf8_char(utf8, &c_size);
  VariableResult vr     = {SERD_SUCCESS, c_size, 0U};

  vr.read_count = c_size;
  if (vr.read_count == 0U) {
    vr.status =
      w_err(writer, SERD_BAD_TEXT, "invalid UTF-8 start: %X", utf8[0]);
  } else if (c <= 0xFFFF) {
    // Write short (4 digit) escape
    if (!(vr = vsink(writer, vr, 2, "\\u")).status &&
        !(vr.status = write_hex_byte(writer, (c & 0xFF00U) >> 8U)) &&
        !(vr.status = write_hex_byte(writer, (c & 0x00FFU)))) {
      vr.write_count += 4U;
    }
  } else {
    // Write long (8 digit) escape
    if (!(vr = vsink(writer, vr, 2, "\\U")).status &&
        !(vr.status = write_hex_byte(writer, (c & 0xFF000000U) >> 24U)) &&
        !(vr.status = write_hex_byte(writer, (c & 0x00FF0000U) >> 16U)) &&
        !(vr.status = write_hex_byte(writer, (c & 0x0000FF00U) >> 8U)) &&
        !(vr.status = write_hex_byte(writer, (c & 0x000000FFU)))) {
      vr.write_count += 8U;
    }
  }

  return vr;
}

static VariableResult
write_text_character(SerdWriter* const writer, const uint8_t* const utf8)
{
  const uint8_t c = utf8[0];
  if ((writer->flags & SERD_WRITE_ASCII) || c < 0x20U || c == 0x7FU) {
    // Write ASCII-compatible UCHAR escape like "\u1234"
    return write_UCHAR(writer, utf8);
  }

  // Parse the leading byte to get the UTF-8 encoding size
  const size_t read_count = utf8_num_bytes(c);
  if (!read_count) {
    const VariableResult result = {SERD_BAD_TEXT, 0U, 0U};
    return result;
  }

  // Write the UTF-8 encoding directly to the output
  const size_t write_count = sink(utf8, read_count, writer);
  SerdStatus   st          = SERD_SUCCESS;
  if (write_count != read_count) {
    st = SERD_BAD_WRITE;
  }

  const VariableResult result = {st, read_count, write_count};
  return result;
}

static VariableResult
write_uri_character(SerdWriter* const writer, const uint8_t* const utf8)
{
  const uint8_t c = utf8[0];
  if (!(c & 0x80U) || (writer->flags & SERD_WRITE_ASCII)) {
    return write_UCHAR(writer, utf8);
  }

  // Parse the leading byte to get the UTF-8 encoding size
  VariableResult result = {SERD_BAD_TEXT, 0U, 0U};
  if ((result.read_count = utf8_num_bytes(c))) {
    // Write the UTF-8 encoding directly to the output
    result.write_count = sink(utf8, result.read_count, writer);
    result.status =
      (result.write_count == result.read_count) ? SERD_SUCCESS : SERD_BAD_WRITE;
  }

  return result;
}

ZIX_NODISCARD static bool
uri_must_escape(const uint8_t c)
{
  return (c == '"') || (c == '<') || (c == '>') || (c == '\\') || (c == '^') ||
         (c == '`') || in_range(c, '{', '}') || !in_range(c, 0x21, 0x7E);
}

static size_t
next_text_index(const char* const   utf8,
                const size_t        begin,
                const size_t        end,
                const BytePredicate predicate)
{
  size_t i = begin;
  while (i < end && !predicate((uint8_t)utf8[i])) {
    ++i;
  }
  return i;
}

static VariableResult
write_uri_text(SerdWriter* const writer,
               const char* const utf8,
               const size_t      n_bytes)
{
  VariableResult result = {SERD_SUCCESS, 0U, 0U};

  for (size_t i = 0; i < n_bytes;) {
    // Write leading chunk as a single fast bulk write
    const size_t j = next_text_index(utf8, i, n_bytes, uri_must_escape);
    result.status  = esink(&utf8[i], j - i, writer);
    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    // Write character (escape or UTF-8)
    const VariableResult r =
      write_uri_character(writer, (const uint8_t*)utf8 + i);
    i += r.read_count;
    result.write_count += r.write_count;
    if (r.status && !(writer->flags & SERD_WRITE_LAX)) {
      result.status = r.status;
      break;
    }

    if (!r.read_count) {
      // Corrupt input, write percent-encoded bytes and scan to next start
      for (uint8_t c = (uint8_t)utf8[i];
           !result.status && i < n_bytes && !is_utf8_leading(c);
           c = (uint8_t)utf8[++i]) {
        result = vsink(writer, result, 1, "%");
        if (!(result.status = write_hex_byte(writer, c))) {
          result.write_count += 2U;
        }
      }
    }
  }

  return result;
}

ZIX_NODISCARD static SerdStatus
ewrite_uri(SerdWriter* const writer, const ZixStringView string)
{
  const VariableResult r = write_uri_text(writer, string.data, string.length);

  return (r.status == SERD_BAD_WRITE || !(writer->flags & SERD_WRITE_LAX))
           ? r.status
           : SERD_SUCCESS;
}

ZIX_NODISCARD static SerdStatus
write_utf8_percent_escape(SerdWriter* const writer,
                          const char* const utf8,
                          const size_t      n_bytes)
{
  SerdStatus st = SERD_SUCCESS;

  for (size_t i = 0U; i < n_bytes; ++i) {
    TRY(st, esink("%", 1, writer));
    TRY(st, write_hex_byte(writer, (uint8_t)utf8[i]));
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_PN_LOCAL_ESC(SerdWriter* const writer, const char c)
{
  const char buf[2] = {'\\', c};

  return esink(buf, sizeof(buf), writer);
}

ZIX_NODISCARD static SerdStatus
write_lname_escape(SerdWriter* const writer,
                   const char* const utf8,
                   const size_t      n_bytes)
{
  return is_PN_LOCAL_ESC(utf8[0])
           ? write_PN_LOCAL_ESC(writer, utf8[0])
           : write_utf8_percent_escape(writer, utf8, n_bytes);
}

ZIX_NODISCARD static SerdStatus
write_lname(SerdWriter* const writer, const ZixStringView string)
{
  const uint8_t* const utf8    = (const uint8_t*)string.data;
  const size_t         n_bytes = string.length;
  SerdStatus           st      = SERD_SUCCESS;
  if (!n_bytes) {
    return st;
  }

  /* The grammar for prefixed names is unfortunately complicated.  We need to
     handle the first character separately, and take care to only escape where
     necessary. */

  // Write first character
  uint8_t   first_size = 0U;
  const int first      = (int)parse_utf8_char(utf8, &first_size);
  if (is_PN_CHARS_U(first) || first == ':' || is_digit(first)) {
    TRY(st, esink(utf8, first_size, writer));
  } else {
    TRY(st, write_lname_escape(writer, string.data, first_size));
  }

  // Write middle and last characters
  for (size_t i = first_size; i < n_bytes;) {
    uint8_t   c_size = 0U;
    const int c      = (int)parse_utf8_char(utf8 + i, &c_size);

    if (is_PN_CHARS(c) || c == ':' || (c == '.' && (i + 1U < n_bytes))) {
      TRY(st, esink(&utf8[i], c_size, writer));
    } else {
      TRY(st, write_lname_escape(writer, &string.data[i], c_size));
    }

    i += c_size;
  }

  return st;
}

ZIX_NODISCARD static size_t
write_long_string_escape(SerdWriter* const writer, const char c)
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

  case '"':
    return sink("\\\"", 2, writer);

  default:
    break;
  }

  return 0;
}

ZIX_NODISCARD static size_t
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

ZIX_NODISCARD static bool
text_must_escape(const uint8_t c)
{
  return c == '\\' || c == '"' || !in_range(c, 0x20, 0x7E);
}

static VariableResult
write_literal_character(SerdWriter* const writer,
                        const char* const utf8,
                        const size_t      i,
                        const size_t      n_bytes)
{
  VariableResult vr = {SERD_SUCCESS, 0U, 0U};

  vr = write_text_character(writer, (const uint8_t*)utf8 + i);
  if (!vr.read_count && (writer->flags & SERD_WRITE_LAX)) {
    // Corrupt input, write replacement char and scan to the next start
    vr.status     = esink(replacement_char, sizeof(replacement_char), writer);
    vr.read_count = next_text_index(utf8, i, n_bytes, is_utf8_leading);
  }

  return vr;
}

ZIX_NODISCARD static SerdStatus
write_short_text(SerdWriter* const writer,
                 const char* const utf8,
                 const size_t      n_bytes)
{
  VariableResult vr = {SERD_SUCCESS, 0U, 0U};
  for (size_t i = 0; !vr.status && i < n_bytes;) {
    // Write leading chunk as a single fast bulk write
    const size_t j = next_text_index(utf8, i, n_bytes, text_must_escape);
    vr.status      = esink(&utf8[i], j - i, writer);
    if (vr.status || ((i = j) == n_bytes)) {
      break; // Error or reached end
    }

    // Try to write character as a special escape
    const size_t escape_len = write_short_string_escape(writer, utf8[i]);
    if (!escape_len) {
      // No special escape for this character, write full Unicode escape
      vr = write_literal_character(writer, utf8, i, n_bytes);
      i += vr.read_count;
    } else {
      ++i;
    }
  }

  return vr.status;
}

ZIX_NODISCARD static SerdStatus
write_long_text(SerdWriter*       writer,
                const char* const utf8,
                const size_t      n_bytes)
{
  size_t         n_quotes = 0;
  VariableResult vr       = {SERD_SUCCESS, 0U, 0U};
  for (size_t i = 0; !vr.status && i < n_bytes;) {
    if (utf8[i] != '"') {
      n_quotes = 0;
    }

    // Write leading chunk as a single fast bulk write
    const size_t j = next_text_index(utf8, i, n_bytes, text_must_escape);
    vr.status      = esink(&utf8[i], j - i, writer);
    if (vr.status || ((i = j) == n_bytes)) {
      break; // Error or reached end
    }

    // Update consecutive quote count
    n_quotes = (utf8[i] == '"') ? (n_quotes + 1U) : 0;

    // Try to write character as a special escape
    const char   c         = utf8[i];
    const bool   raw_quote = (n_quotes && n_quotes < 3 && i + 1U != n_bytes);
    const size_t escape_len =
      raw_quote ? sink(&c, 1, writer) : write_long_string_escape(writer, c);

    if (!escape_len) {
      // No special escape for this character, write full Unicode escape
      vr = write_literal_character(writer, utf8, i, n_bytes);
      i += vr.read_count;
    } else {
      ++i;
    }
  }

  return vr.status;
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
  const VariableResult  r       = write_uri_text(writer, (const char*)buf, len);

  context->status = r.status;
  return r.write_count;
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

  // Adjust indent, but tolerate if it would become negative
  if (rule->indent && (rule->flags & (PRE_LINE | POST_LINE))) {
    writer->indent += rule->indent;
  }

  // Adjust indentation for object comma if necessary
  if (sep == SEP_END_O_N && !writer->context.comma_indented) {
    ++writer->indent;
    writer->context.comma_indented = true;
  } else if (sep == SEP_END_P && writer->context.comma_indented) {
    --writer->indent;
    writer->context.comma_indented = false;
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
    writer->indent                 = is_set(writer, SERD_GRAPH) ? 1 : 0;
    writer->context.comma_indented = false;
    TRY(st, esink("\n", 1, writer));
  }

  writer->last_sep = sep;
  return st;
}

static void
free_anon_stack(SerdWriter* const writer)
{
  while (!serd_stack_is_empty(&writer->anon_stack)) {
    pop_context(writer);
  }
}

static SerdStatus
reset_context(SerdWriter* const writer, const unsigned flags)
{
  free_anon_stack(writer);

  if (flags & RESET_GRAPH) {
    writer->context.graph.type = SERD_NOTHING;
  }

  if (flags & RESET_INDENT) {
    writer->indent = 0;
  }

  writer->context.type           = CTX_NAMED;
  writer->context.subject.type   = SERD_NOTHING;
  writer->context.predicate.type = SERD_NOTHING;
  writer->context.comma_indented = false;
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

static SerdStatus
write_IRIREF(SerdWriter* const writer, const ZixStringView string)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, esink("<", 1, writer));

  // Write the string and return early if resolution is disabled or impossible
  const SerdURIView base_uri = serd_env_base_uri_view(writer->env);
  if ((writer->flags & SERD_WRITE_UNRESOLVED) || !base_uri.scheme.length) {
    TRY(st, ewrite_uri(writer, string));
    return esink(">", 1, writer);
  }

  // Resolve the input URI reference to a (hopefully) absolute URI
  const SerdURIView in_uri  = serd_parse_uri(string.data);
  SerdURIView       out_uri = serd_resolve_uri(in_uri, base_uri);

  // Determine if the absolute URI should be written, or make it relative again
  const bool write_abs =
    !supports_abbrev(writer) ||
    (writer->root_uri_string && !uri_is_under(&out_uri, &writer->root_uri));
  if (!write_abs) {
    out_uri = serd_relative_uri(in_uri, base_uri);
  }

  UriSinkContext context = {writer, SERD_SUCCESS};
  serd_write_uri(out_uri, uri_sink, &context);

  return context.status ? context.status : esink(">", 1, writer);
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
        !serd_env_qualify(writer->env, string, &pair)) {
      TRY(st, write_lname(writer, pair.prefix));
      TRY(st, esink(":", 1, writer));
      return write_lname(writer, pair.suffix);
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

  return write_IRIREF(writer, string);
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
    TRY(st, write_long_text(writer, string.data, string.length));
    st = esink("\"\"\"", 3, writer);
  } else {
    TRY(st, esink("\"", 1, writer));
    TRY(st, write_short_text(writer, string.data, string.length));
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
write_blank(SerdWriter* const        writer,
            const ZixStringView      label,
            const SerdField          field,
            const SerdStatementFlags flags)
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
write_token(SerdWriter* const        writer,
            const SerdField          field,
            const SerdStatementFlags statement_flags,
            const SerdTokenView      node)
{
  return (node.type == SERD_URI)     ? write_uri(writer, node.string)
         : (node.type == SERD_CURIE) ? write_curie(writer, node.string)
         : (node.type == SERD_BLANK)
           ? write_blank(writer, node.string, field, statement_flags)
           : SERD_BAD_ARG;
}

ZIX_NODISCARD static SerdStatus
write_object(SerdWriter* const        writer,
             const SerdStatementFlags statement_flags,
             const SerdObjectView     node)
{
  return (node.type == SERD_LITERAL)
           ? write_literal(writer, node.string, node.flags, node.meta)
           : write_token(writer,
                         SERD_OBJECT,
                         statement_flags,
                         serd_object_token_view(node));
}

ZIX_NODISCARD static SerdStatus
write_pred(SerdWriter* writer, const SerdTokenView pred)
{
  ZixAllocator* const allocator = serd_world_allocator(writer->world);

  SerdStatus st = token_equals_symbol(pred, RDF_TYPE) ? esink("a", 1, writer)
                                                      : write_iri(writer, pred);

  if (!st) {
    st = write_sep(writer, SEP_P_O);
  }

  set_current(allocator, &writer->context.predicate, pred);
  writer->context.comma_indented = false;
  return st;
}

ZIX_NODISCARD static SerdStatus
write_list_next(SerdWriter* const        writer,
                const SerdStatementFlags flags,
                const SerdTokenView      predicate,
                const SerdObjectView     object)
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

  if (is_set(writer, SERD_SUBJECT)) {
    TRY(st, write_sep(writer, SEP_STOP));
  }

  if (is_set(writer, SERD_GRAPH)) {
    TRY(st, write_sep(writer, SEP_GRAPH_R));
  }

  return st;
}

SerdStatus
serd_writer_write_statement(SerdWriter* const        writer,
                            const SerdStatementFlags flags,
                            SerdTokenView            graph,
                            const SerdTokenView      subject,
                            const SerdTokenView      predicate,
                            const SerdObjectView     object)
{
  assert(writer);

  ZixAllocator* const allocator = serd_world_allocator(writer->world);

  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_SUCCESS;
  }

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
  if ((has_graph && !current_field_equals(writer, SERD_GRAPH, graph)) ||
      (!has_graph && is_set(writer, SERD_GRAPH))) {
    TRY(st, terminate_context(writer));
    reset_context(writer, RESET_GRAPH | RESET_INDENT);
    TRY(st, write_newline(writer));
    if (has_graph) {
      TRY(st, write_token(writer, SERD_GRAPH, flags, graph));
      TRY(st, write_sep(writer, SEP_GRAPH_L));
      set_current(allocator, &writer->context.graph, graph);
    }
  }

  if (writer->context.type == CTX_LIST) {
    // Continue a list
    if (token_equals_symbol(predicate, RDF_FIRST) &&
        token_equals_symbol(serd_object_token_view(object), RDF_NIL)) {
      return esink("()", 2, writer);
    }

    TRY_FAILING(st, write_list_next(writer, flags, predicate, object));

    if (st == SERD_FAILURE) { // Reached end of list
      pop_context(writer);
      return SERD_SUCCESS;
    }

  } else if (current_field_equals(writer, SERD_SUBJECT, subject)) {
    if (current_field_equals(writer, SERD_PREDICATE, predicate)) {
      // Elide S P (write O)

      const Sep  last        = writer->last_sep;
      const bool before_name = !(flags & (SERD_ANON_O | SERD_LIST_O));
      const bool after_end   = (last == SEP_ANON_R) || (last == SEP_LIST_R);
      TRY(st,
          write_sep(writer,
                    before_name ? SEP_END_O_N
                    : after_end ? SEP_END_O_AA
                                : SEP_END_O_NA));

    } else {
      // Elide S (write P and O)

      const bool first = !is_set(writer, SERD_PREDICATE);
      TRY(st, write_sep(writer, first ? SEP_S_P : SEP_END_P));
      TRY(st, write_pred(writer, predicate));
    }

    TRY(st, write_object(writer, flags, object));

  } else {
    // No abbreviation

    if (!serd_stack_is_empty(&writer->anon_stack)) {
      return SERD_BAD_ARG;
    }

    if (is_set(writer, SERD_SUBJECT)) {
      TRY(st, write_sep(writer, SEP_STOP));
    }

    if (writer->last_sep == SEP_STOP) {
      TRY(st, write_newline(writer));
    }

    TRY(st, write_token(writer, SERD_SUBJECT, flags, subject));
    if (!(flags & SERD_LIST_S)) {
      TRY(st, write_sep(writer, SEP_S_P));
    }

    reset_context(writer, 0U);
    set_current(allocator, &writer->context.subject, subject);

    if (!(flags & SERD_LIST_S)) {
      TRY(st, write_pred(writer, predicate));
    }

    TRY(st, write_object(writer, flags, object));
  }

  if (flags & (SERD_ANON_S | SERD_LIST_S)) {
    // Push context for anonymous or list subject
    const bool is_list = (flags & SERD_LIST_S);
    push_context(writer,
                 is_list ? CTX_LIST : CTX_BLANK,
                 graph,
                 subject,
                 is_list ? serd_no_token() : predicate);
  }

  if (flags & (SERD_ANON_O | SERD_LIST_O)) {
    // Push context for anonymous or list object if necessary
    const SerdTokenView anon_subject = {object.type, object.string};
    push_context(writer,
                 (flags & SERD_LIST_O) ? CTX_LIST : CTX_BLANK,
                 graph,
                 anon_subject,
                 serd_no_token());
  }

  return st;
}

SerdStatus
serd_writer_end_anon(SerdWriter* const writer, const ZixStringView label)
{
  assert(writer);

  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax != SERD_TURTLE && writer->syntax != SERD_TRIG) {
    return SERD_SUCCESS;
  }

  if (serd_stack_is_empty(&writer->anon_stack)) {
    return w_err(writer, SERD_BAD_CALL, "unexpected end of anonymous node");
  }

  // Decrease indent if we're current comma-indented (multiple objects at end)
  if (writer->context.comma_indented) {
    assert(writer->indent);
    --writer->indent;
    writer->context.comma_indented = false;
  }

  // Write the end separator ']' and pop the context
  TRY(st, write_sep(writer, SEP_ANON_R));
  pop_context(writer);

  const SerdTokenView node = {SERD_BLANK, label};
  if (is_set(writer, SERD_PREDICATE) &&
      current_field_equals(writer, SERD_SUBJECT, node)) {
    // Now-finished anonymous node is the new subject with no other context
    writer->context.predicate.type = SERD_NOTHING;
  }

  return st;
}

SerdStatus
serd_writer_finish(SerdWriter* const writer)
{
  assert(writer);

  const SerdStatus st0 = terminate_context(writer);
  const SerdStatus st1 = serd_byte_sink_flush(&writer->byte_sink);
  free_anon_stack(writer);
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

  SerdWriter* writer =
    (SerdWriter*)zix_calloc(allocator, 1, sizeof(SerdWriter));
  if (!writer) {
    return NULL;
  }

  writer->world  = world;
  writer->syntax = syntax;
  writer->flags  = flags;
  writer->env    = env;

  writer->root_uri_string = NULL;
  writer->root_uri        = serd_empty_uri();

  writer->anon_stack = serd_stack_new(allocator, SERD_PAGE_SIZE);
  if (!writer->anon_stack.buf) {
    zix_free(allocator, writer);
    return NULL;
  }

  writer->byte_sink = serd_byte_sink_new(
    allocator, ssink, stream, (flags & SERD_WRITE_BULK) ? SERD_PAGE_SIZE : 1);

  if ((flags & SERD_WRITE_BULK) && !writer->byte_sink.buf) {
    serd_stack_free(&writer->anon_stack);
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

SerdStatus
serd_writer_set_base_uri(SerdWriter* const writer, const ZixStringView uri)
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
    const bool had_subject = writer->context.subject.type;
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

SerdStatus
serd_writer_set_prefix(SerdWriter* const   writer,
                       const ZixStringView name,
                       const ZixStringView uri)
{
  assert(writer);

  SerdStatus st = serd_env_set_prefix(writer->env, name, uri);
  if (st) {
    return st == SERD_NO_CHANGE ? SERD_SUCCESS : st;
  }

  if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
    const bool had_subject = writer->context.subject.type;
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
  free_context(writer);
  free_anon_stack(writer);
  serd_stack_free(&writer->anon_stack);
  zix_free(allocator, writer->bprefix);
  serd_byte_sink_free(allocator, &writer->byte_sink);
  zix_free(allocator, writer->root_uri_string);
  zix_free(allocator, writer);
}

SerdEnv*
serd_writer_env(SerdWriter* const writer)
{
  assert(writer);
  return writer->env;
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
