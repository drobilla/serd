// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "block_dumper.h"
#include "memory.h"
#include "namespaces.h"
#include "node.h"
#include "sink.h"
#include "string_utils.h"
#include "try.h"
#include "turtle.h"
#include "uri_utils.h"
#include "warnings.h"
#include "world.h"

#include "serd/env.h"
#include "serd/event.h"
#include "serd/field.h"
#include "serd/node.h"
#include "serd/object_view.h"
#include "serd/output_stream.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "serd/syntax.h"
#include "serd/token_view.h"
#include "serd/uri.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
  CTX_NAMED, ///< Normal non-anonymous context
  CTX_BLANK, ///< Anonymous blank node
  CTX_LIST,  ///< Anonymous list
} ContextType;

typedef struct {
  ContextType             type;
  SerdStatementEventFlags flags;
  SerdNode*               graph;
  SerdNode*               subject;
  SerdNode*               predicate;
  bool                    predicates;
  bool                    comma_indented;
} WriteContext;

/// A status for an operation that reads/writes variable numbers of bytes
typedef struct {
  SerdStatus status;
  size_t     read_count;
  size_t     write_count;
} VariableResult;

static const WriteContext WRITE_CONTEXT_NULL =
  {CTX_NAMED, 0U, NULL, NULL, NULL, 0U, 0U};

static const ZixStringView rdf_nil   = ZIX_STATIC_STRING(NS_RDF "nil");
static const ZixStringView rdf_first = ZIX_STATIC_STRING(NS_RDF "first");
static const ZixStringView rdf_type  = ZIX_STATIC_STRING(NS_RDF "type");

typedef enum {
  SEP_NONE,        ///< Sentinel after "nothing"
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
  SerdSink        iface;
  SerdWorld*      world;
  SerdSyntax      syntax;
  SerdWriterFlags flags;
  const SerdEnv*  env;
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

typedef bool (*const BytePredicate)(uint8_t) ZIX_NODISCARD;

ZIX_NODISCARD static SerdStatus
serd_writer_set_base_uri(SerdWriter* writer, ZixStringView uri);

ZIX_NODISCARD static SerdStatus
serd_writer_set_prefix(SerdWriter*   writer,
                       ZixStringView name,
                       ZixStringView uri);

ZIX_NODISCARD static SerdStatus
write_iri(SerdWriter* writer, SerdNodeType type, ZixStringView string);

ZIX_NODISCARD static SerdStatus
write_object(SerdWriter*             writer,
             SerdStatementEventFlags statement_flags,
             SerdObjectView          node);

ZIX_NODISCARD static SerdStatus
write_uri_node(SerdWriter* writer, ZixStringView string);

ZIX_NODISCARD static bool
supports_abbrev(const SerdWriter* writer)
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

ZIX_LOG_FUNC(3, 4)
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

  return node && serd_node_type(node) ? node : NULL;
}

ZIX_NODISCARD static SerdStatus
push_context(SerdWriter* const             writer,
             const ContextType             type,
             const SerdStatementEventFlags statement_flags,
             const SerdTokenView           graph,
             const SerdTokenView           subject,
             const SerdTokenView           predicate)
{
  ZixAllocator* const allocator = writer->world->allocator;

  // Push the current context to the stack

  if (writer->anon_stack_size >= writer->max_depth) {
    return SERD_BAD_STACK;
  }

  writer->anon_stack[writer->anon_stack_size++] = writer->context;

  // Update the current context

  const WriteContext current = {
    type,
    statement_flags,

    serd_field_supports(SERD_GRAPH, graph.type)
      ? serd_node_new(allocator, serd_a_token(graph.type, graph.string))
      : NULL,

    serd_node_new(allocator, serd_a_token(subject.type, subject.string)),

    serd_field_supports(SERD_PREDICATE, predicate.type)
      ? serd_node_new(allocator, serd_a_token(predicate.type, predicate.string))
      : NULL,

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

ZIX_NODISCARD static inline SerdStreamResult
wsink(const void* buf, size_t len, SerdWriter* writer)
{
  return serd_block_dumper_write(&writer->output, buf, len);
}

ZIX_NODISCARD static inline SerdStatus
esink(const void* buf, size_t len, SerdWriter* writer)
{
  return serd_block_dumper_write(&writer->output, buf, len).status;
}

ZIX_NODISCARD static inline VariableResult
add_wresult(const VariableResult vr, const SerdStreamResult wr)
{
  const VariableResult r = {
    wr.status, vr.read_count, vr.write_count + wr.count};
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

  SerdStreamResult wr = {SERD_SUCCESS, 0U};
  if (c_size == 0U) {
    wr = wsink(replacement_char, sizeof(replacement_char), writer);
    if (!wr.status) {
      wr.status = w_err(writer, SERD_BAD_TEXT, "bad UTF-8 start: %X", utf8[0]);
    }
  } else if (c <= 0xFFFF) {
    // Write short (4 digit) escape
    if (!(wr = wsink("\\u", 2, writer)).status &&
        !(wr.status = write_hex_byte(writer, (c & 0xFF00U) >> 8U)) &&
        !(wr.status = write_hex_byte(writer, (c & 0x00FFU)))) {
      wr.count += 4U;
    }
  } else {
    // Write long (8 digit) escape
    if (!(wr = wsink("\\U", 2, writer)).status &&
        !(wr.status = write_hex_byte(writer, (c & 0xFF000000U) >> 24U)) &&
        !(wr.status = write_hex_byte(writer, (c & 0x00FF0000U) >> 16U)) &&
        !(wr.status = write_hex_byte(writer, (c & 0x0000FF00U) >> 8U)) &&
        !(wr.status = write_hex_byte(writer, (c & 0x000000FFU)))) {
      wr.count += 8U;
    }
  }

  const VariableResult vr = {wr.status, c_size, wr.count};
  return vr;
}

static VariableResult
write_text_character(SerdWriter* const writer, const uint8_t* const utf8)
{
  VariableResult result = {SERD_SUCCESS, 0U, 0U};
  const uint8_t  c      = utf8[0];

  if ((writer->flags & SERD_WRITE_ESCAPED) || c < 0x20U || c == 0x7FU) {
    // Write ASCII-compatible UCHAR escape like "\u1234"
    return write_UCHAR(writer, utf8);
  }

  // Parse the leading byte to get the UTF-8 encoding size
  if (!(result.read_count = utf8_num_bytes(c))) {
    result.status = SERD_BAD_TEXT;
    return result;
  }

  // Write the UTF-8 encoding directly to the output
  return add_wresult(result, wsink(utf8, result.read_count, writer));
}

static VariableResult
write_uri_character(SerdWriter* const writer, const uint8_t* const utf8)
{
  const uint8_t c = utf8[0];

  if ((c & 0x80U) && !(writer->flags & SERD_WRITE_ESCAPED)) {
    VariableResult result = {SERD_SUCCESS, 0U, 0U};

    // Parse the leading byte to get the UTF-8 encoding size
    if (!(result.read_count = utf8_num_bytes(c))) {
      result.status = SERD_BAD_TEXT;
    } else {
      // Write the UTF-8 encoding directly to the output
      result = add_wresult(result, wsink(utf8, result.read_count, writer));
    }

    return result;
  }

  return write_UCHAR(writer, utf8);
}

static bool
uri_must_escape(const uint8_t c)
{
  return (c == '"') || (c == '<') || (c == '>') || (c == '\\') || (c == '^') ||
         (c == '`') || in_range(c, '{', '}') || !in_range(c, 0x21, 0x7E);
}

static size_t
next_text_index(const char*         utf8,
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
write_uri(SerdWriter* const writer,
          const char* const string,
          const size_t      n_bytes)
{
  const uint8_t* const utf8 = (const uint8_t*)string;

  VariableResult result = {SERD_SUCCESS, 0U, 0U};

  for (size_t i = 0; !result.status && i < n_bytes;) {
    // Write leading chunk as a single fast bulk write
    const size_t j   = next_text_index(string, i, n_bytes, uri_must_escape);
    const size_t len = j - i;
    result.status    = esink(&string[i], len, writer);
    result.read_count += len;
    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    // Write character (escape or UTF-8)
    const VariableResult r = write_uri_character(writer, utf8 + i);
    assert(r.status || r.read_count > 0);
    i += r.read_count;
    result.write_count += r.write_count;
    result.status = r.status;
  }

  return result;
}

ZIX_NODISCARD static SerdStatus
ewrite_uri(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
  return write_uri(writer, utf8, n_bytes).status;
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
write_lname_escape(SerdWriter* writer, const char* const utf8, size_t n_bytes)
{
  return is_PN_LOCAL_ESC(utf8[0])
           ? write_PN_LOCAL_ESC(writer, utf8[0])
           : write_utf8_percent_escape(writer, utf8, n_bytes);
}

ZIX_NODISCARD static SerdStatus
write_lname(SerdWriter* writer, const char* utf8, const size_t n_bytes)
{
  SerdStatus st = SERD_SUCCESS;
  if (!n_bytes) {
    return st;
  }

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

ZIX_NODISCARD static SerdStatus
write_long_string_escape(SerdWriter* const writer,
                         const size_t      n_consecutive_quotes,
                         const bool        is_last,
                         const char        c)
{
  switch (c) {
  case '\\':
    return esink("\\\\", 2, writer);

  case '\b':
    return esink("\\b", 2, writer);

  case '\n':
  case '\r':
  case '\t':
  case '\f':
    return esink(&c, 1, writer); // Write character as-is

  case '\"':
    if (n_consecutive_quotes >= 3 || is_last) {
      // Two quotes in a row, or quote at string end, escape
      return esink("\\\"", 2, writer);
    }

    return esink(&c, 1, writer);

  default:
    break;
  }

  return SERD_FAILURE;
}

ZIX_NODISCARD static SerdStatus
write_short_string_escape(SerdWriter* const writer, const char c)
{
  switch (c) {
  case '\\':
    return esink("\\\\", 2, writer);
  case '\n':
    return esink("\\n", 2, writer);
  case '\r':
    return esink("\\r", 2, writer);
  case '\t':
    return (writer->flags & SERD_WRITE_ESCAPED) ? esink("\\t", 2, writer)
                                                : esink("\t", 1, writer);
  case '"':
    return esink("\\\"", 2, writer);
  default:
    break;
  }

  if (!(writer->flags & SERD_WRITE_ESCAPED)) {
    // These are written with UCHAR in pre-NTriples test cases format
    switch (c) {
    case '\b':
      return esink("\\b", 2, writer);
    case '\f':
      return esink("\\f", 2, writer);
    default:
      break;
    }
  }

  return SERD_FAILURE;
}

ZIX_NODISCARD static bool
text_is_special_normal(const uint8_t c)
{
  return c == '\\' || c == '"' || (c != '\t' && !in_range(c, 0x20, 0x7E));
}

ZIX_NODISCARD static bool
text_is_special_escaped(const uint8_t c)
{
  return c == '\\' || c == '"' || !in_range(c, 0x20, 0x7E);
}

ZIX_NODISCARD static SerdStatus
write_short_text(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
  const BytePredicate is_special = (writer->flags & SERD_WRITE_ESCAPED)
                                     ? text_is_special_escaped
                                     : text_is_special_normal;

  const bool     lax = (writer->flags & SERD_WRITE_LAX);
  VariableResult vr  = {SERD_SUCCESS, 0U, 0U};
  for (size_t i = 0; !vr.status && i < n_bytes;) {
    // Write leading chunk as a single fast bulk write
    const size_t j = next_text_index(utf8, i, n_bytes, is_special);
    vr.status      = esink(&utf8[i], j - i, writer);
    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    // Try to write character as a special short escape (newline and friends)
    const char in = utf8[i];
    if (!(vr.status = write_short_string_escape(writer, in))) {
      vr.read_count = 1U;
    } else if (vr.status == SERD_FAILURE) {
      // No special escape for this character, write full Unicode escape
      vr = write_text_character(writer, (const uint8_t*)utf8 + i);
    }

    if (!vr.read_count) {
      i         = next_text_index(utf8, i + 1U, n_bytes, is_utf8_leading);
      vr.status = lax ? SERD_SUCCESS : vr.status;
    } else {
      i += vr.read_count;
    }
  }

  return vr.status;
}

ZIX_NODISCARD static SerdStatus
write_long_text(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
  const BytePredicate text_is_special = (writer->flags & SERD_WRITE_ESCAPED)
                                          ? text_is_special_escaped
                                          : text_is_special_normal;

  const bool     lax      = (writer->flags & SERD_WRITE_LAX);
  size_t         n_quotes = 0;
  VariableResult vr       = {SERD_SUCCESS, 0U, 0U};
  for (size_t i = 0; !vr.status && i < n_bytes;) {
    if (utf8[i] != '"') {
      n_quotes = 0;
    }

    // Write leading chunk as a single fast bulk write
    const size_t j = next_text_index(utf8, i, n_bytes, text_is_special);
    vr.status      = esink(&utf8[i], j - i, writer);
    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    const bool last = i + 1U == n_bytes;
    n_quotes        = (utf8[i] == '\"') ? (n_quotes + 1U) : 0U;

    // Try to write character as a special long escape (newline and friends)
    vr.status = write_long_string_escape(writer, n_quotes, last, utf8[i]);
    if (!vr.status) {
      vr.read_count = 1U;
    } else if (vr.status == SERD_FAILURE) {
      // No special escape for this character, write full Unicode escape
      vr = write_UCHAR(writer, (const uint8_t*)utf8 + i);
    }

    if (!vr.read_count) {
      i         = next_text_index(utf8, i + 1U, n_bytes, is_utf8_leading);
      vr.status = lax ? SERD_SUCCESS : vr.status;
    } else {
      i += vr.read_count;
    }
  }

  return vr.status;
}

typedef struct {
  SerdWriter* writer;
  SerdStatus  status;
} UriSinkContext;

ZIX_NODISCARD static size_t
uri_sink(const void* buf, size_t size, size_t nmemb, void* stream)
{
  (void)size;
  assert(size == 1);

  UriSinkContext* const context = (UriSinkContext*)stream;
  SerdWriter* const     writer  = context->writer;
  const VariableResult  r       = write_uri(writer, (const char*)buf, nmemb);

  context->status = r.status;
  return r.write_count;
}

ZIX_NODISCARD static SerdStatus
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

ZIX_NODISCARD static SerdStatus
write_top_level_sep(SerdWriter* writer)
{
  return (writer->last_sep && !(writer->flags & SERD_WRITE_TERSE))
           ? write_newline(writer, false)
           : SERD_SUCCESS;
}

static int
adjust_indent(const int current, const int delta)
{
  return (delta >= 0 || current >= -delta) ? current + delta : 0;
}

ZIX_NODISCARD static SerdStatus
write_sep(SerdWriter* writer, const SerdStatementEventFlags flags, Sep sep)
{
  SerdStatus           st   = SERD_SUCCESS;
  const SepRule* const rule = &rules[sep];

  const SepMask last_sep_mask = (1U << writer->last_sep);
  const bool    pre_line      = (rule->pre_line_after & last_sep_mask);
  const bool    post_line     = (rule->post_line_after & last_sep_mask);
  const bool    terse         = (flags & (SERD_TERSE_S | SERD_TERSE_O));

  if (terse && sep >= SEP_LIST_BEGIN && sep <= SEP_LIST_END) {
    sep = (Sep)((int)sep + 3); // Switch to corresponding terse separator
  }

  // Adjust indent, but tolerate if it would become negative
  if (rule->indent && (pre_line || post_line)) {
    writer->indent = adjust_indent(writer->indent, rule->indent);
  }

  // If this is the first comma, bump the increment for the following object
  if (sep == SEP_END_O && !writer->context.comma_indented) {
    ++writer->indent;
    writer->context.comma_indented = true;
  }

  // Write newline or space before separator if necessary
  if (pre_line) {
    TRY(st, write_newline(writer, terse));
  } else if (rule->pre_space_after & last_sep_mask) {
    TRY(st, esink(" ", 1, writer));
  }

  // Write actual separator string
  if (rule->sep) {
    TRY(st, esink(&rule->sep, 1, writer));
  }

  // Write newline after separator if necessary
  if (post_line) {
    TRY(st, write_newline(writer, terse));
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
    serd_node_set_header(writer->context.predicate, 0U, 0U, (SerdNodeType)0U);
  }

  if (writer->context.subject) {
    serd_node_set_header(writer->context.subject, 0U, 0U, (SerdNodeType)0U);
  }

  if (flags & RESET_GRAPH) {
    if (writer->context.graph) {
      serd_node_set_header(writer->context.graph, 0U, 0U, (SerdNodeType)0U);
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

// Return the name of the XSD datatype referred to by `datatype`, if any
static const char*
get_xsd_name(const SerdEnv* const env, const SerdTokenView datatype)
{
  if (datatype.type == SERD_URI &&
      (!strncmp(datatype.string.data, NS_XSD, sizeof(NS_XSD) - 1))) {
    return datatype.string.data + sizeof(NS_XSD) - 1U;
  }

  if (datatype.type == SERD_CURIE) {
    ZixStringView prefix;
    ZixStringView suffix;
    // We can be a bit lazy/presumptive here due to grammar limitations
    if (!serd_env_expand(env, datatype.string, &prefix, &suffix)) {
      if (!strcmp((const char*)prefix.data, NS_XSD)) {
        return (const char*)suffix.data;
      }
    }
  }

  return "";
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

  if (supports_abbrev(writer) && (node_flags & SERD_IS_LONG)) {
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
    st = write_iri(writer, meta.type, meta.string);
  }

  return st;
}

static SerdStatus
write_IRIREF(SerdWriter* const writer, const ZixStringView string)
{
  SerdStatus st       = SERD_SUCCESS;
  const bool verbatim = (writer->flags & SERD_WRITE_VERBATIM);

  TRY(st, esink("<", 1, writer));

  if (verbatim || !serd_env_base_uri_view(writer->env).scheme.length) {
    // Resolution disabled or we have no base URI, simply write the node
    TRY(st, ewrite_uri(writer, string.data, string.length));
    return esink(">", 1, writer);
  }

  // Resolve the input node URI reference to a (hopefully) absolute URI
  const SerdURIView base_uri = serd_env_base_uri_view(writer->env);
  SerdURIView       uri      = serd_parse_uri(string.data);
  SerdURIView       abs_uri  = serd_resolve_uri(uri, base_uri);

  // Determine if we should write the absolute URI or make it relative again
  const bool         base_rooted = uri_is_under(&base_uri, &writer->root_uri);
  const SerdURIView* root        = base_rooted ? &writer->root_uri : &base_uri;
  const bool         rooted      = uri_is_under(&abs_uri, root);
  const bool         write_abs   = !supports_abbrev(writer) || !rooted;

  UriSinkContext context = {writer, SERD_SUCCESS};
  if (write_abs) {
    serd_write_uri(abs_uri, uri_sink, &context);
  } else {
    serd_write_uri(serd_relative_uri(uri, base_uri), uri_sink, &context);
  }

  return context.status ? context.status : esink(">", 1, writer);
}

ZIX_NODISCARD static SerdStatus
write_uri_node(SerdWriter* const writer, const ZixStringView string)
{
  SerdStatus st         = SERD_SUCCESS;
  const bool has_scheme = serd_uri_string_has_scheme(string.data);

  if (supports_abbrev(writer)) {
    if (!strcmp(string.data, NS_RDF "nil")) {
      return esink("()", 2, writer);
    }

    ZixStringView prefix = {NULL, 0};
    ZixStringView suffix = {NULL, 0};
    if (has_scheme && !(writer->flags & SERD_WRITE_EXPANDED) &&
        !serd_env_qualify(writer->env, string, &prefix, &suffix)) {
      TRY(st, write_lname(writer, prefix.data, prefix.length));
      TRY(st, esink(":", 1, writer));
      return write_lname(writer, suffix.data, suffix.length);
    }
  }

  if (!has_scheme &&
      (writer->syntax == SERD_NTRIPLES || writer->syntax == SERD_NQUADS) &&
      !serd_env_base_uri_view(writer->env).scheme.length) {
    return w_err(writer,
                 SERD_BAD_ARG,
                 "syntax does not support URI reference <%s>",
                 string.data);
  }

  return write_IRIREF(writer, string);
}

ZIX_NODISCARD static SerdStatus
write_curie(SerdWriter* const writer, const ZixStringView curie)
{
  writer->last_sep = SEP_NONE;

  if (supports_abbrev(writer)) {
    return write_lname(writer, curie.data, curie.length);
  }

  ZixStringView prefix = {NULL, 0};
  ZixStringView suffix = {NULL, 0};
  SerdStatus    st     = SERD_SUCCESS;
  if ((st = serd_env_expand(writer->env, curie, &prefix, &suffix))) {
    return w_err(writer, st, "unknown namespace prefix in '%s'", curie.data);
  }

  TRY(st, esink("<", 1, writer));
  TRY(st, ewrite_uri(writer, prefix.data, prefix.length));
  TRY(st, ewrite_uri(writer, suffix.data, suffix.length));
  return esink(">", 1, writer);
}

ZIX_NODISCARD static SerdStatus
write_blank(SerdWriter* const             writer,
            const ZixStringView           label,
            const SerdField               field,
            const SerdStatementEventFlags flags)
{
  SerdStatus st = SERD_SUCCESS;

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
        (field == SERD_OBJECT && (flags & SERD_EMPTY_O)) ||
        (field == SERD_GRAPH && (flags & SERD_EMPTY_G))) {
      return esink("[]", 2, writer);
    }
  }

  TRY(st, esink("_:", 2, writer));
  return esink(label.data, label.length, writer);
}

ZIX_NODISCARD static SerdStatus
write_variable(SerdWriter* const writer, const ZixStringView label)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, esink("?", 1, writer));
  TRY(st, esink(label.data, label.length, writer));

  writer->last_sep = SEP_NONE;
  return st;
}

static SerdStatus
write_iri(SerdWriter* const   writer,
          const SerdNodeType  type,
          const ZixStringView string)
{
  return (type == SERD_CURIE) ? write_curie(writer, string)
                              : write_uri_node(writer, string);
}

ZIX_NODISCARD static SerdStatus
write_token(SerdWriter* const             writer,
            const SerdField               field,
            const SerdStatementEventFlags statement_flags,
            const SerdTokenView           node)
{
  return (node.type == SERD_URI)     ? write_uri_node(writer, node.string)
         : (node.type == SERD_CURIE) ? write_curie(writer, node.string)
         : (node.type == SERD_BLANK)
           ? write_blank(writer, node.string, field, statement_flags)
         : (node.type == SERD_VARIABLE) ? write_variable(writer, node.string)
                                        : SERD_BAD_ARG;
}

ZIX_NODISCARD static SerdStatus
write_predicate(SerdWriter* const writer, const SerdTokenView node)
{
  return (node.type == SERD_URI)        ? write_uri_node(writer, node.string)
         : (node.type == SERD_CURIE)    ? write_curie(writer, node.string)
         : (node.type == SERD_VARIABLE) ? write_variable(writer, node.string)
                                        : SERD_BAD_ARG;
}

ZIX_NODISCARD static SerdStatus
write_verb(SerdWriter* const writer, const SerdTokenView node)
{
  return (node.type == SERD_URI && supports_abbrev(writer) &&
          !(writer->flags & SERD_WRITE_LONGHAND) &&
          zix_string_view_equals(node.string, rdf_type))
           ? esink("a", 1, writer)
           : write_predicate(writer, node);
}

ZIX_NODISCARD static SerdStatus
write_object(SerdWriter* const             writer,
             const SerdStatementEventFlags statement_flags,
             const SerdObjectView          node)
{
  return (node.type == SERD_LITERAL)
           ? write_literal(writer, node.string, node.flags, node.meta)
         : (node.type == SERD_URI)   ? write_uri_node(writer, node.string)
         : (node.type == SERD_CURIE) ? write_curie(writer, node.string)
         : (node.type == SERD_BLANK)
           ? write_blank(writer, node.string, SERD_OBJECT, statement_flags)
         : (node.type == SERD_VARIABLE) ? write_variable(writer, node.string)
                                        : SERD_SUCCESS;
}

ZIX_NODISCARD static SerdStatus
write_pred(SerdWriter*             writer,
           SerdStatementEventFlags flags,
           const SerdTokenView     pred)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, write_verb(writer, pred));
  TRY(st, write_sep(writer, flags, SEP_P_O));

  writer->context.predicates     = true;
  writer->context.comma_indented = false;
  return serd_node_set(
    writer->world->allocator, &writer->context.predicate, pred);
}

ZIX_NODISCARD static SerdStatus
write_list_next(SerdWriter* const             writer,
                const SerdStatementEventFlags flags,
                const SerdTokenView           predicate,
                const SerdObjectView          object)
{
  SerdStatus st = SERD_SUCCESS;

  if (zix_string_view_equals(object.string, rdf_nil)) {
    TRY(st, write_sep(writer, writer->context.flags, SEP_LIST_END));
    return SERD_FAILURE;
  }

  if (zix_string_view_equals(predicate.string, rdf_first)) {
    TRY(st, write_object(writer, flags, object));
  } else {
    TRY(st, write_sep(writer, writer->context.flags, SEP_LIST_SEP));
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
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

ZIX_NODISCARD static SerdStatus
write_ntriples_statement(SerdWriter* const             writer,
                         const SerdStatementEventFlags flags,
                         const SerdTokenView           subject,
                         const SerdTokenView           predicate,
                         const SerdObjectView          object)
{
  assert(writer);

  SerdStatus st = SERD_SUCCESS;

  TRY(st, write_token(writer, SERD_SUBJECT, flags, subject));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_predicate(writer, predicate));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_object(writer, flags, object));
  TRY(st, esink(" .\n", 3, writer));

  return st;
}

static SerdStatus
write_nquads_statement(SerdWriter* const             writer,
                       const SerdStatementEventFlags flags,
                       const SerdTokenView           subject,
                       const SerdTokenView           predicate,
                       const SerdObjectView          object,
                       const SerdTokenView           graph)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, write_token(writer, SERD_SUBJECT, flags, subject));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_predicate(writer, predicate));
  TRY(st, esink(" ", 1, writer));
  TRY(st, write_object(writer, flags, object));

  if (graph.type != SERD_LITERAL) {
    TRY(st, esink(" ", 1, writer));
    TRY(st, write_token(writer, SERD_GRAPH, flags, graph));
  }

  return esink(" .\n", 3, writer);
}

static SerdStatus
update_abbreviation_context(SerdWriter* const             writer,
                            const SerdStatementEventFlags flags,
                            const SerdTokenView           subject,
                            const SerdTokenView           predicate,
                            const SerdObjectView          object,
                            const SerdTokenView           graph)
{
  static const SerdTokenView no_predicate = {ZIX_STATIC_STRING(""),
                                             SERD_LITERAL};

  SerdStatus st = SERD_SUCCESS;

  // Push context for list or anonymous subject if necessary
  if (flags & SERD_ANON_S) {
    st = push_context(writer, CTX_BLANK, flags, graph, subject, predicate);
  } else if (flags & SERD_LIST_S) {
    st = push_context(writer, CTX_LIST, flags, graph, subject, no_predicate);
  }

  // Push context for list or anonymous object if necessary
  if (!st) {
    const SerdTokenView object_token = {object.string, object.type};
    if (flags & SERD_ANON_O) {
      st = push_context(
        writer, CTX_BLANK, flags, graph, object_token, no_predicate);
    } else if (flags & SERD_LIST_O) {
      st = push_context(
        writer, CTX_LIST, flags, graph, object_token, no_predicate);
    }
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_list_statement(SerdWriter* const             writer,
                     const SerdStatementEventFlags flags,
                     const SerdTokenView           subject,
                     const SerdTokenView           predicate,
                     const SerdObjectView          object,
                     const SerdTokenView           graph)
{
  SerdStatus st = SERD_SUCCESS;

  if (zix_string_view_equals(predicate.string, rdf_first) &&
      zix_string_view_equals(object.string, rdf_nil)) {
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

ZIX_NODISCARD static SerdStatus
write_inline_predicate(SerdWriter* const       writer,
                       SerdStatementEventFlags flags,
                       const SerdTokenView     predicate)
{
  SerdStatus st = SERD_SUCCESS;

  if (serd_node_equals_token_view(writer->context.predicate, predicate)) {
    // Elide S and P (write only a separator here)

    const Sep  last      = writer->last_sep;
    const bool open_o    = flags & (SERD_ANON_O | SERD_LIST_O);
    const bool after_end = (last == SEP_ANON_END) || (last == SEP_LIST_END);

    TRY(st,
        write_sep(writer,
                  flags,
                  after_end ? (open_o ? SEP_JOIN_O_AA : SEP_JOIN_O_AN)
                            : (open_o ? SEP_JOIN_O_NA : SEP_END_O)));

  } else {
    // Elide only S (write a separator and P here)

    if (writer->context.comma_indented && !(flags & SERD_ANON_S)) {
      --writer->indent;
      writer->context.comma_indented = false;
    }

    const bool first = !ctx(writer, SERD_PREDICATE);
    TRY(st, write_sep(writer, flags, first ? SEP_S_P : SEP_END_P));
    TRY(st, write_pred(writer, flags, predicate));
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
write_turtle_trig_statement(SerdWriter* const       writer,
                            SerdStatementEventFlags flags,
                            const SerdTokenView     subject,
                            const SerdTokenView     predicate,
                            const SerdObjectView    object,
                            const SerdTokenView     graph)
{
  SerdStatus st = SERD_SUCCESS;

  if ((flags & SERD_LIST_O) && zix_string_view_equals(object.string, rdf_nil)) {
    /* Tolerate LIST_O_BEGIN for "()" objects, even though it doesn't make
       much sense, because older versions handled this gracefully.  Consider
       making this an error in a later major version. */
    flags &= (SerdStatementEventFlags)~SERD_LIST_O;
  }

  if (writer->context.type == CTX_LIST) {
    return write_list_statement(
      writer, flags, subject, predicate, object, graph);
  }

  // Write subject and/or predicate if necessary
  if (serd_node_equals_token_view(writer->context.subject, subject)) {
    // Elide subject
    TRY(st, write_inline_predicate(writer, flags, predicate));
  } else {
    // No abbreviation

    if (writer->anon_stack_size) {
      return SERD_BAD_ARG;
    }

    if (ctx(writer, SERD_SUBJECT)) {
      TRY(st, write_sep(writer, flags, SEP_END_S));
    }

    if (writer->last_sep == SEP_END_S || writer->last_sep == SEP_END_DIRECT) {
      TRY(st, write_top_level_sep(writer));
    }

    // Write subject node
    TRY(st, write_token(writer, SERD_SUBJECT, flags, subject));
    if (flags & SERD_ANON_S) {
      TRY(st, write_sep(writer, flags, SEP_ANON_S_P));
    } else if (!(flags & SERD_LIST_S)) {
      TRY(st, write_sep(writer, flags, SEP_S_P));
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

  TRY(st, write_object(writer, flags, object));

  return update_abbreviation_context(
    writer, flags, subject, predicate, object, graph);
}

ZIX_NODISCARD static SerdStatus
write_trig_statement(SerdWriter* const             writer,
                     const SerdStatementEventFlags flags,
                     const SerdTokenView           subject,
                     const SerdTokenView           predicate,
                     const SerdObjectView          object,
                     const SerdTokenView           graph)
{
  SerdStatus st = SERD_SUCCESS;

  const bool has_graph = serd_field_supports(SERD_GRAPH, graph.type);

  if ((!has_graph && writer->context.graph) ||
      (has_graph &&
       (!writer->context.graph ||
        !serd_node_equals_token_view(writer->context.graph, graph)))) {
    TRY(st, terminate_context(writer));
    TRY(st, write_top_level_sep(writer));
    reset_context(writer, true);

    if (serd_field_supports(SERD_GRAPH, graph.type)) {
      TRY(st, write_token(writer, SERD_GRAPH, flags, graph));
      TRY(st, write_sep(writer, flags, SEP_GRAPH_BEGIN));
      serd_node_set(writer->world->allocator, &writer->context.graph, graph);
    }
  }

  return write_turtle_trig_statement(
    writer, flags, subject, predicate, object, graph);
}

ZIX_NODISCARD static SerdStatus
serd_writer_write_statement(SerdWriter* const       writer,
                            SerdStatementEventFlags flags,
                            const SerdStatementView statement)
{
  static const SerdTokenView no_graph = {ZIX_STATIC_STRING(""), SERD_LITERAL};

  const SerdTokenView  subject   = statement.subject;
  const SerdTokenView  predicate = statement.predicate;
  const SerdObjectView object    = statement.object;
  const SerdTokenView  graph     = statement.graph;

  if (!serd_field_supports(SERD_SUBJECT, subject.type) ||
      !serd_field_supports(SERD_PREDICATE, predicate.type) ||
      ((flags & SERD_ANON_S) && (flags & SERD_LIST_S)) ||   // Nonsense
      ((flags & SERD_EMPTY_S) && (flags & SERD_LIST_S)) ||  // Nonsense
      ((flags & SERD_ANON_O) && (flags & SERD_LIST_O)) ||   // Nonsense
      ((flags & SERD_EMPTY_O) && (flags & SERD_LIST_O)) ||  // Nonsense
      ((flags & SERD_ANON_S) && (flags & SERD_TERSE_S)) ||  // Unsupported
      ((flags & SERD_ANON_O) && (flags & SERD_TERSE_O)) ||  // Unsupported
      ((flags & SERD_TERSE_S) && !(flags & SERD_LIST_S)) || // Unsupported
      ((flags & SERD_TERSE_O) && !(flags & SERD_LIST_O))) { // Unsupported
    return SERD_BAD_ARG;
  }

  switch (writer->syntax) {
  case SERD_SYNTAX_EMPTY:
    break;

  case SERD_TURTLE:
    return write_turtle_trig_statement(
      writer, flags, subject, predicate, object, no_graph);

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

ZIX_NODISCARD static SerdStatus
serd_writer_end_anon(SerdWriter* const writer, const ZixStringView label)
{
  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax != SERD_TURTLE && writer->syntax != SERD_TRIG) {
    return SERD_SUCCESS;
  }

  if (!writer->anon_stack_size) {
    return w_err(writer, SERD_BAD_EVENT, "unexpected end of anonymous node");
  }

  // Write the end separator ']' and pop the context
  TRY(st, write_sep(writer, writer->context.flags, SEP_ANON_END));
  pop_context(writer);

  if (writer->context.predicate &&
      zix_string_view_equals(label,
                             serd_node_string_view(writer->context.subject))) {
    // Now-finished anonymous node is the new subject with no other context
    serd_node_set_header(writer->context.predicate, 0U, 0U, (SerdNodeType)0U);
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
serd_writer_on_event(void* const handle, const SerdEvent* const event)
{
  SerdWriter* const writer = (SerdWriter*)handle;
  assert(writer);

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
    return serd_writer_end_anon(writer, event->end.label);
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
                const SerdEnv*    env,
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
  writer->iface.on_event  = serd_writer_on_event;

  return writer;
}

ZIX_NODISCARD static SerdStatus
serd_writer_set_base_uri(SerdWriter* const writer, const ZixStringView uri)
{
  SERD_DISABLE_NULL_WARNINGS

  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
    TRY(st, terminate_context(writer));
    TRY(st, esink("@base <", 7, writer));
    TRY(st, esink(uri.data, uri.length, writer));
    TRY(st, esink(">", 1, writer));
    TRY(st, write_sep(writer, writer->context.flags, SEP_END_DIRECT));
  }

  SERD_RESTORE_WARNINGS

  return reset_context(writer, RESET_GRAPH | RESET_INDENT);
}

ZIX_NODISCARD SerdStatus
serd_writer_set_root_uri(SerdWriter* const writer, const ZixStringView uri)
{
  assert(writer);

  ZixAllocator* const allocator = writer->world->allocator;

  serd_node_free(allocator, writer->root_node);
  writer->root_node = NULL;
  writer->root_uri  = SERD_URI_NULL;

  if (uri.length) {
    writer->root_node = serd_node_new(allocator, serd_a_uri(uri));
    writer->root_uri  = serd_node_uri_view(writer->root_node);
  }

  return SERD_SUCCESS;
}

ZIX_NODISCARD SerdStatus
serd_writer_set_prefix(SerdWriter* const   writer,
                       const ZixStringView name,
                       const ZixStringView uri)
{
  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
    TRY(st, terminate_context(writer));
    if (writer->flags & SERD_WRITE_CONTEXTUAL) {
      return st;
    }

    TRY(st, esink("@prefix ", 8, writer));
    TRY(st, esink(name.data, name.length, writer));
    TRY(st, esink(": <", 3, writer));
    TRY(st, ewrite_uri(writer, uri.data, uri.length));
    TRY(st, esink(">", 1, writer));
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
