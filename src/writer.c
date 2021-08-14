/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "writer.h"

#include "block_dumper.h"
#include "env.h"
#include "node.h"
#include "sink.h"
#include "string_utils.h"
#include "system.h"
#include "try.h"
#include "turtle.h"
#include "uri_utils.h"
#include "world.h"

#include "serd/serd.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#  define SERD_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#  define SERD_WARN_UNUSED_RESULT
#endif

typedef enum {
  CTX_NAMED, ///< Normal non-anonymous context
  CTX_BLANK, ///< Anonymous blank node
  CTX_LIST   ///< Anonymous list
} ContextType;

typedef struct {
  ContextType        type;
  SerdStatementFlags flags;
  SerdNode*          graph;
  SerdNode*          subject;
  SerdNode*          predicate;
  bool               indented_object;
} WriteContext;

static const WriteContext WRITE_CONTEXT_NULL =
  {CTX_NAMED, 0, NULL, NULL, NULL, false};

static const size_t anon_stack_capacity = SERD_PAGE_SIZE / sizeof(WriteContext);

typedef enum {
  SEP_NONE,        ///< Placeholder for nodes or nothing
  SEP_END_S,       ///< End of a subject ('.')
  SEP_END_P,       ///< End of a predicate (';')
  SEP_END_O,       ///< End of an object (',')
  SEP_S_P,         ///< Between a subject and predicate (whitespace)
  SEP_P_O,         ///< Between a predicate and object (whitespace)
  SEP_ANON_BEGIN,  ///< Start of anonymous node ('[')
  SEP_ANON_S_P,    ///< Between start of anonymous node and predicate
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

#define SEP_ALL ((SepMask)-1)
#define M(s) (1U << (s))

typedef struct {
  const char* str;             ///< Sep string
  size_t      len;             ///< Length of sep string
  int         indent;          ///< Indent delta
  SepMask     pre_space_after; ///< Leading space if after given seps
  SepMask     pre_line_after;  ///< Leading newline if after given seps
  SepMask     post_line_after; ///< Trailing newline if after given seps
} SepRule;

static const SepRule rules[] = {
  {"", 0, +0, SEP_NONE, SEP_NONE, SEP_NONE},
  {".\n", 2, -1, SEP_ALL, SEP_NONE, SEP_NONE},
  {";", 1, +0, SEP_ALL, SEP_NONE, SEP_ALL},
  {",", 1, +0, SEP_ALL, SEP_NONE, ~(M(SEP_ANON_END) | M(SEP_LIST_END))},
  {"", 0, +1, SEP_NONE, SEP_NONE, SEP_ALL},
  {" ", 1, +0, SEP_NONE, SEP_NONE, SEP_NONE},
  {"[", 1, +1, M(SEP_END_O), M(SEP_TLIST_BEGIN) | M(SEP_TLIST_SEP), SEP_NONE},
  {"", 0, +0, SEP_NONE, SEP_ALL, SEP_NONE},
  {"]", 1, -1, SEP_NONE, ~M(SEP_ANON_BEGIN), SEP_NONE},
  {"(", 1, +1, M(SEP_END_O), SEP_NONE, SEP_ALL},
  {"", 0, +0, SEP_NONE, SEP_ALL, SEP_NONE},
  {")", 1, -1, SEP_NONE, SEP_ALL, SEP_NONE},
  {"(", 1, +1, SEP_NONE, SEP_NONE, SEP_NONE},
  {"", 0, +0, SEP_ALL, SEP_NONE, SEP_NONE},
  {")", 1, -1, SEP_NONE, SEP_NONE, SEP_NONE},
  {"{", 1, +1, SEP_ALL, SEP_NONE, SEP_ALL},
  {"}", 1, -1, SEP_NONE, SEP_NONE, SEP_ALL},
};

struct SerdWriterImpl {
  SerdWorld*      world;
  SerdSink        iface;
  SerdSyntax      syntax;
  SerdWriterFlags flags;
  const SerdEnv*  env;
  SerdNode*       root_node;
  SerdURIView     root_uri;
  WriteContext*   anon_stack;
  size_t          anon_stack_size;
  SerdBlockDumper output;
  WriteContext    context;
  Sep             last_sep;
  int             indent;
  bool            empty;
};

typedef enum { WRITE_STRING, WRITE_LONG_STRING } TextContext;

static SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri);

SERD_WARN_UNUSED_RESULT static SerdStatus
write_node(SerdWriter*        writer,
           const SerdNode*    node,
           SerdField          field,
           SerdStatementFlags flags);

SERD_LOG_FUNC(3, 4)
static SerdStatus
w_err(SerdWriter* const writer, const SerdStatus st, const char* const fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  serd_vlogf(writer->world, SERD_LOG_LEVEL_ERROR, fmt, args);

  va_end(args);
  return st;
}

static bool
supports_abbrev(const SerdWriter* writer)
{
  return writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG;
}

static bool
supports_uriref(const SerdWriter* writer)
{
  return writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG;
}

static SerdStatus
free_context(SerdWriter* writer)
{
  serd_node_free(writer->context.graph);
  serd_node_free(writer->context.subject);
  serd_node_free(writer->context.predicate);
  return SERD_SUCCESS;
}

static SerdStatus
push_context(SerdWriter* const        writer,
             const ContextType        type,
             const SerdStatementFlags flags,
             const SerdNode* const    g,
             const SerdNode* const    s,
             const SerdNode* const    p)
{
  if (writer->anon_stack_size >= anon_stack_capacity) {
    return SERD_ERR_OVERFLOW;
  }

  const WriteContext new_context = {type,
                                    flags,
                                    serd_node_copy(g),
                                    serd_node_copy(s),
                                    serd_node_copy(p),
                                    false};

  writer->anon_stack[writer->anon_stack_size++] = writer->context;
  writer->context                               = new_context;
  return SERD_SUCCESS;
}

static void
start_object_indent(SerdWriter* const writer)
{
  if (!writer->context.indented_object) {
    ++writer->indent;
    writer->context.indented_object = true;
  }
}

static void
end_object_indent(SerdWriter* const writer)
{
  if (writer->context.indented_object && writer->indent > 0) {
    --writer->indent;
    writer->context.indented_object = false;
  }
}

static void
pop_context(SerdWriter* writer)
{
  assert(writer->anon_stack_size > 0);
  end_object_indent(writer);
  free_context(writer);
  writer->context = writer->anon_stack[--writer->anon_stack_size];
}

static SerdNode*
ctx(SerdWriter* writer, const SerdField field)
{
  SerdNode* node = NULL;
  if (field == SERD_SUBJECT) {
    node = writer->context.subject;
  } else if (field == SERD_PREDICATE) {
    node = writer->context.predicate;
  } else if (field == SERD_GRAPH) {
    node = writer->context.graph;
  }

  return node && node->type ? node : NULL;
}

SERD_WARN_UNUSED_RESULT static size_t
sink(const void* buf, size_t len, SerdWriter* writer)
{
  const size_t written = serd_block_dumper_write(buf, 1, len, &writer->output);

  if (written != len) {
    if (errno) {
      char message[1024] = {0};
      serd_system_strerror(errno, message, sizeof(message));

      w_err(writer, SERD_ERR_BAD_WRITE, "write error (%s)", message);
    } else {
      w_err(writer,
            SERD_ERR_BAD_WRITE,
            "unknown write error, %zu / %zu bytes written",
            written,
            len);
    }
  }

  return written;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
esink(const void* buf, size_t len, SerdWriter* writer)
{
  return sink(buf, len, writer) == len ? SERD_SUCCESS : SERD_ERR_BAD_WRITE;
}

// Write a single character as a Unicode escape
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
    w_err(writer, SERD_ERR_BAD_TEXT, "invalid UTF-8 start: %X", utf8[0]);
    *st = SERD_ERR_BAD_TEXT;
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
      *st = SERD_ERR_BAD_WRITE;
      return len;
    }

    if ((i = j) == n_bytes) {
      break; // Reached end
    }

    // Write UTF-8 character
    size_t size = 0;
    len += write_character(writer, (const uint8_t*)utf8 + i, &size, st);
    i += size;
    if (*st && !(writer->flags & SERD_WRITE_LAX)) {
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

SERD_WARN_UNUSED_RESULT static SerdStatus
ewrite_uri(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
  SerdStatus st = SERD_SUCCESS;
  write_uri(writer, utf8, n_bytes, &st);

  return (st == SERD_ERR_BAD_WRITE || !(writer->flags & SERD_WRITE_LAX))
           ? st
           : SERD_SUCCESS;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_uri_from_node(SerdWriter* writer, const SerdNode* node)
{
  return ewrite_uri(writer, serd_node_string(node), node->length);
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_utf8_percent_escape(SerdWriter* const writer,
                          const char* const utf8,
                          const size_t      n_bytes)
{
  SerdStatus st        = SERD_SUCCESS;
  char       escape[4] = {0, 0, 0, 0};

  for (size_t i = 0u; i < n_bytes; ++i) {
    snprintf(escape, sizeof(escape), "%%%02X", (uint8_t)utf8[i]);
    TRY(st, esink(escape, 3, writer));
  }

  return st;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_PN_LOCAL_ESC(SerdWriter* const writer, const char c)
{
  SerdStatus st = SERD_SUCCESS;

  if (!(st = esink("\\", 1, writer))) {
    st = esink(&c, 1, writer);
  }

  return st;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_lname_escape(SerdWriter* writer, const char* const utf8, size_t n_bytes)
{
  SerdStatus st = SERD_SUCCESS;

  if (is_PN_LOCAL_ESC(utf8[0])) {
    st = write_PN_LOCAL_ESC(writer, utf8[0]);
  } else {
    st = write_utf8_percent_escape(writer, utf8, n_bytes);
  }

  return st;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_lname(SerdWriter* writer, const char* utf8, size_t n_bytes)
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
  size_t    first_size = 0u;
  const int first = (int)parse_utf8_char((const uint8_t*)utf8, &first_size);
  if (is_PN_CHARS_U(first) || first == ':' || is_digit(first)) {
    st = esink(utf8, first_size, writer);
  } else {
    st = write_lname_escape(writer, utf8, first_size);
  }

  // Write middle characters
  size_t i = first_size;
  while (!st && i < n_bytes - 1u) {
    size_t    c_size = 0u;
    const int c      = (int)parse_utf8_char((const uint8_t*)utf8 + i, &c_size);
    if (i + c_size >= n_bytes) {
      break;
    }

    if (is_PN_CHARS(c) || c == '.' || c == ':') {
      st = esink(&utf8[i], c_size, writer);
    } else {
      st = write_lname_escape(writer, &utf8[i], c_size);
    }

    i += c_size;
  }

  // Write last character
  if (!st && i < n_bytes) {
    size_t    last_size = 0u;
    const int last = (int)parse_utf8_char((const uint8_t*)utf8 + i, &last_size);
    if (is_PN_CHARS(last) || last == ':') {
      st = esink(&utf8[i], last_size, writer);
    } else {
      st = write_lname_escape(writer, &utf8[i], last_size);
    }
  }

  return st;
}

SERD_WARN_UNUSED_RESULT static size_t
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

SERD_WARN_UNUSED_RESULT static size_t
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

SERD_WARN_UNUSED_RESULT static SerdStatus
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
      size_t size = 0;
      write_character(writer, (const uint8_t*)utf8 + i - 1, &size, &st);
      if (st && !(writer->flags & SERD_WRITE_LAX)) {
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
  }

  return SERD_SUCCESS;
}

typedef struct {
  SerdWriter* writer;
  SerdStatus  status;
} UriSinkContext;

SERD_WARN_UNUSED_RESULT static size_t
uri_sink(const void* buf, size_t size, size_t nmemb, void* stream)
{
  (void)size;
  assert(size == 1);

  UriSinkContext* const context = (UriSinkContext*)stream;
  SerdWriter* const     writer  = context->writer;

  return write_uri(writer, (const char*)buf, nmemb, &context->status);
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_newline(SerdWriter* writer, bool terse)
{
  if (terse || (writer->flags & SERD_WRITE_TERSE)) {
    return esink(" ", 1, writer);
  }

  SerdStatus st = SERD_SUCCESS;
  TRY(st, esink("\n", 1, writer));
  for (int i = 0; i < writer->indent; ++i) {
    TRY(st, esink("\t", 1, writer));
  }

  return st;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_top_level_sep(SerdWriter* writer)
{
  return ((!writer->empty && !(writer->flags & SERD_WRITE_TERSE))
            ? write_newline(writer, false)
            : SERD_SUCCESS);
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_sep(SerdWriter* writer, const SerdStatementFlags flags, Sep sep)
{
  SerdStatus           st   = SERD_SUCCESS;
  const SepRule* const rule = &rules[sep];

  const bool terse = (((flags & SERD_TERSE_S) && (flags & SERD_LIST_S)) ||
                      ((flags & SERD_TERSE_O) && (flags & SERD_LIST_O)));

  if (terse && sep >= SEP_LIST_BEGIN && sep <= SEP_LIST_END) {
    sep = (Sep)((int)sep + 3); // Switch to corresponding terse separator
  }

  // Adjust indent, but tolerate if it would become negative
  if ((rule->pre_line_after & (1u << writer->last_sep)) ||
      (rule->post_line_after & (1u << writer->last_sep))) {
    writer->indent = ((rule->indent >= 0 || writer->indent >= -rule->indent)
                        ? writer->indent + rule->indent
                        : 0);
  }

  // Write newline or space before separator if necessary
  if (rule->pre_line_after & (1u << writer->last_sep)) {
    TRY(st, write_newline(writer, terse));
  } else if (rule->pre_space_after & (1u << writer->last_sep)) {
    TRY(st, esink(" ", 1, writer));
  }

  // Write actual separator string
  TRY(st, esink(rule->str, rule->len, writer));

  // Write newline after separator if necessary
  if (rule->post_line_after & (1u << writer->last_sep)) {
    TRY(st, write_newline(writer, terse));
    writer->last_sep = SEP_NONE;
  } else {
    writer->last_sep = sep;
  }

  if (sep == SEP_END_S) {
    writer->indent = 0;
  }

  return st;
}

static void
reset_context(SerdWriter* writer, const bool including_graph)
{
  // Free any lingering contexts in case there was an error
  while (writer->anon_stack_size > 0) {
    pop_context(writer);
  }

  if (including_graph && writer->context.graph) {
    memset(writer->context.graph, 0, sizeof(SerdNode));
  }

  if (writer->context.subject) {
    memset(writer->context.subject, 0, sizeof(SerdNode));
  }

  if (writer->context.predicate) {
    memset(writer->context.predicate, 0, sizeof(SerdNode));
  }

  writer->anon_stack_size         = 0;
  writer->context.indented_object = false;
  writer->empty                   = false;

  if (including_graph) {
    writer->indent = 0;
  }
}

static bool
is_inline_start(const SerdWriter*  writer,
                SerdField          field,
                SerdStatementFlags flags)
{
  return (supports_abbrev(writer) &&
          ((field == SERD_SUBJECT && (flags & SERD_ANON_S)) ||
           (field == SERD_OBJECT && (flags & SERD_ANON_O))));
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_literal(SerdWriter* const        writer,
              const SerdNode* const    node,
              const SerdStatementFlags flags)
{
  writer->last_sep = SEP_NONE;

  const SerdNode* datatype = serd_node_datatype(node);
  const SerdNode* lang     = serd_node_language(node);
  const char*     node_str = serd_node_string(node);
  const char*     type_uri = datatype ? serd_node_string(datatype) : NULL;
  if (supports_abbrev(writer) && type_uri) {
    if (serd_node_equals(datatype, writer->world->xsd_boolean) ||
        serd_node_equals(datatype, writer->world->xsd_integer)) {
      return esink(node_str, node->length, writer);
    }

    if (serd_node_equals(datatype, writer->world->xsd_decimal) &&
        strchr(node_str, '.') && node_str[node->length - 1] != '.') {
      /* xsd:decimal literals without trailing digits, e.g. "5.", can
         not be written bare in Turtle.  We could add a 0 which is
         prettier, but changes the text and breaks round tripping.
      */
      return esink(node_str, node->length, writer);
    }
  }

  SerdStatus st = SERD_SUCCESS;
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

SERD_WARN_UNUSED_RESULT static SerdStatus
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

  return st ? st : esink(">", 1, writer);
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_uri_node(SerdWriter* const     writer,
               const SerdNode* const node,
               const SerdField       field)
{
  SerdStatus           st         = SERD_SUCCESS;
  SerdStringView       prefix     = {NULL, 0};
  SerdStringView       suffix     = {NULL, 0};
  const SerdStringView node_view  = serd_node_string_view(node);
  const char*          node_str   = serd_node_string(node);
  const bool           has_scheme = serd_uri_string_has_scheme(node_str);
  if (supports_abbrev(writer)) {
    if (!(writer->flags & SERD_WRITE_RDF_TYPE) && field == SERD_PREDICATE &&
        serd_node_equals(node, writer->world->rdf_type)) {
      return esink("a", 1, writer);
    }

    if (serd_node_equals(node, writer->world->rdf_nil)) {
      return esink("()", 2, writer);
    }

    if (has_scheme && !(writer->flags & SERD_WRITE_EXPANDED) &&
        !serd_env_qualify(writer->env, node_view, &prefix, &suffix)) {
      TRY(st, write_lname(writer, prefix.buf, prefix.len));
      TRY(st, esink(":", 1, writer));
      return write_lname(writer, suffix.buf, suffix.len);
    }
  }

  if (!has_scheme && !supports_uriref(writer) &&
      !serd_env_base_uri(writer->env)) {
    return w_err(writer,
                 SERD_ERR_BAD_ARG,
                 "syntax does not support URI reference <%s>",
                 node_str);
  }

  return write_full_uri_node(writer, node);
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_blank(SerdWriter* const        writer,
            const SerdNode*          node,
            const SerdField          field,
            const SerdStatementFlags flags)
{
  const char* node_str = serd_node_string(node);
  if (supports_abbrev(writer)) {
    if (is_inline_start(writer, field, flags)) {
      return write_sep(writer, flags, SEP_ANON_BEGIN);
    }

    if ((field == SERD_SUBJECT && (flags & SERD_LIST_S)) ||
        (field == SERD_OBJECT && (flags & SERD_LIST_O))) {
      return write_sep(writer, flags, SEP_LIST_BEGIN);
    }

    if ((field == SERD_SUBJECT && (flags & SERD_EMPTY_S)) ||
        (field == SERD_GRAPH && (flags & SERD_EMPTY_G))) {
      writer->last_sep = SEP_NONE; // Treat "[]" like a node
      return esink("[]", 2, writer);
    }
  }

  SerdStatus st = SERD_SUCCESS;
  TRY(st, esink("_:", 2, writer));
  TRY(st, esink(node_str, node->length, writer));

  writer->last_sep = SEP_NONE;
  return st;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_variable(SerdWriter* const writer, const SerdNode* const node)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, esink("?", 1, writer));
  TRY(st, esink(serd_node_string(node), node->length, writer));

  writer->last_sep = SEP_NONE;
  return st;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
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
  case SERD_BLANK:
    st = write_blank(writer, node, field, flags);
    break;
  case SERD_VARIABLE:
    st = write_variable(writer, node);
    break;
  }

  return st;
}

static bool
is_resource(const SerdNode* node)
{
  return node && node->type > SERD_LITERAL;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_pred(SerdWriter* writer, SerdStatementFlags flags, const SerdNode* pred)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, write_node(writer, pred, SERD_PREDICATE, flags));
  TRY(st, write_sep(writer, flags, SEP_P_O));
  serd_node_set(&writer->context.predicate, pred);
  return st;
}

SERD_WARN_UNUSED_RESULT static SerdStatus
write_list_obj(SerdWriter* const        writer,
               const SerdStatementFlags flags,
               const SerdNode* const    predicate,
               const SerdNode* const    object,
               bool* const              is_end)
{
  if (serd_node_equals(object, writer->world->rdf_nil)) {
    *is_end = true;
    return write_sep(writer, writer->context.flags, SEP_LIST_END);
  }

  return (serd_node_equals(predicate, writer->world->rdf_first)
            ? write_node(writer, object, SERD_OBJECT, flags)
            : write_sep(writer, writer->context.flags, SEP_LIST_SEP));
}

static SerdStatus
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

  TRY(st, esink(" .\n", 3, writer));

  return st;
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
  if (!st) {
    if (flags & SERD_ANON_S) {
      st = push_context(writer, CTX_BLANK, flags, graph, subject, predicate);
    } else if (flags & SERD_LIST_S) {
      st = push_context(writer, CTX_LIST, flags, graph, subject, NULL);
    }
  }

  // Push context for list or anonymous object if necessary
  if (!st) {
    if (flags & SERD_ANON_O) {
      st = push_context(writer, CTX_BLANK, flags, graph, object, NULL);
    } else if (flags & SERD_LIST_O) {
      st = push_context(writer, CTX_LIST, flags, graph, object, NULL);
    }
  }

  // Update current context to this statement if this isn't a new context
  if (!st) {
    if (!(flags & (SERD_ANON_S | SERD_LIST_S | SERD_ANON_O | SERD_LIST_O))) {
      serd_node_set(&writer->context.graph, graph);
      serd_node_set(&writer->context.subject, subject);
      serd_node_set(&writer->context.predicate, predicate);
    }
  }

  return st;
}

static SerdStatus
write_list_statement(SerdWriter* const        writer,
                     const SerdStatementFlags flags,
                     const SerdNode* const    subject,
                     const SerdNode* const    predicate,
                     const SerdNode* const    object,
                     const SerdNode* const    graph)
{
  SerdStatus st     = SERD_SUCCESS;
  bool       is_end = false;

  if (serd_node_equals(predicate, writer->world->rdf_first) &&
      serd_node_equals(object, writer->world->rdf_nil)) {
    return esink("()", 2, writer);
  }

  TRY(st, write_list_obj(writer, flags, predicate, object, &is_end));
  if (is_end) {
    pop_context(writer);
    return SERD_SUCCESS;
  }

  return update_abbreviation_context(
    writer, flags, subject, predicate, object, graph);
}

static SerdStatus
write_turtle_trig_statement(SerdWriter* const        writer,
                            const SerdStatementFlags flags,
                            const SerdNode* const    subject,
                            const SerdNode* const    predicate,
                            const SerdNode* const    object,
                            const SerdNode* const    graph)
{
  SerdStatus st = SERD_SUCCESS;

  if (writer->context.type == CTX_LIST) {
    return write_list_statement(
      writer, flags, subject, predicate, object, graph);
  }

  if (serd_node_equals(subject, writer->context.subject)) {
    if (serd_node_equals(predicate, writer->context.predicate)) {
      // Abbreviate S P
      if (!(flags & (SERD_ANON_O | SERD_LIST_O))) {
        start_object_indent(writer);
      }

      TRY(st, write_sep(writer, writer->context.flags, SEP_END_O));

    } else {
      // Abbreviate S
      end_object_indent(writer);

      const Sep sep = ctx(writer, SERD_PREDICATE) ? SEP_END_P : SEP_S_P;
      TRY(st, write_sep(writer, writer->context.flags, sep));
      TRY(st, write_pred(writer, writer->context.flags, predicate));
    }

  } else {
    // No abbreviation
    end_object_indent(writer);

    if (ctx(writer, SERD_SUBJECT)) {
      // Terminate last subject
      TRY(st, write_sep(writer, writer->context.flags, SEP_END_S));
    }

    // Write new subject
    if (!ctx(writer, SERD_GRAPH)) {
      TRY(st, write_top_level_sep(writer));
    }
    reset_context(writer, false);
    serd_node_set(&writer->context.subject, subject);
    TRY(st, write_node(writer, subject, SERD_SUBJECT, flags));

    // Write appropriate S,P separator based on the context
    if (!(flags & (SERD_ANON_S | SERD_LIST_S))) {
      TRY(st, write_sep(writer, writer->context.flags, SEP_S_P));
    } else if (flags & SERD_ANON_S) {
      TRY(st, write_sep(writer, writer->context.flags, SEP_ANON_S_P));
    }

    if (!(flags & SERD_LIST_S)) {
      TRY(st, write_pred(writer, flags, predicate));
    }
  }

  TRY(st, write_node(writer, object, SERD_OBJECT, flags));

  return update_abbreviation_context(
    writer, flags, subject, predicate, object, graph);
}

static SerdStatus
write_turtle_statement(SerdWriter* const        writer,
                       const SerdStatementFlags flags,
                       const SerdNode* const    subject,
                       const SerdNode* const    predicate,
                       const SerdNode* const    object)
{
  return write_turtle_trig_statement(
    writer, flags, subject, predicate, object, NULL);
}

static SerdStatus
write_trig_statement(SerdWriter* const        writer,
                     const SerdStatementFlags flags,
                     const SerdNode* const    subject,
                     const SerdNode* const    predicate,
                     const SerdNode* const    object,
                     const SerdNode* const    graph)
{
  SerdStatus st = SERD_SUCCESS;

  if (!serd_node_equals(graph, writer->context.graph)) {
    if (ctx(writer, SERD_SUBJECT)) {
      TRY(st, write_sep(writer, writer->context.flags, SEP_END_S));
    }

    if (ctx(writer, SERD_GRAPH)) {
      TRY(st, write_sep(writer, writer->context.flags, SEP_GRAPH_END));
    }

    TRY(st, write_top_level_sep(writer));
    reset_context(writer, true);

    if (graph) {
      TRY(st, write_node(writer, graph, SERD_GRAPH, flags));
      TRY(st, write_sep(writer, flags, SEP_GRAPH_BEGIN));
      serd_node_set(&writer->context.graph, graph);
    }
  }

  return write_turtle_trig_statement(
    writer, flags, subject, predicate, object, graph);
}

static SerdStatus
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
    return SERD_ERR_BAD_ARG;
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

SERD_WARN_UNUSED_RESULT static SerdStatus
serd_writer_end_anon(SerdWriter* writer, const SerdNode* node)
{
  if (writer->syntax != SERD_TURTLE && writer->syntax != SERD_TRIG) {
    return SERD_SUCCESS;
  }

  if (writer->anon_stack_size == 0) {
    return w_err(writer,
                 SERD_ERR_BAD_CALL,
                 "unexpected end of anonymous node `%s'",
                 serd_node_string(node));
  }

  SerdStatus st = write_sep(writer, writer->context.flags, SEP_ANON_END);
  pop_context(writer);

  if (writer->context.predicate &&
      serd_node_equals(node, writer->context.subject)) {
    // Now-finished anonymous node is the new subject with no other context
    memset(writer->context.predicate, 0, sizeof(SerdNode));
  }

  return st;
}

static SerdStatus
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

  return SERD_ERR_BAD_ARG;
}

SerdStatus
serd_writer_write_node(SerdWriter* writer, const SerdNode* node)
{
  return write_node(writer, node, SERD_OBJECT, 0);
}

SerdStatus
serd_writer_finish(SerdWriter* writer)
{
  assert(writer);

  SerdStatus st = SERD_SUCCESS;
  if (ctx(writer, SERD_SUBJECT)) {
    st = write_sep(writer, writer->context.flags, SEP_END_S);
  }

  if (!st && ctx(writer, SERD_GRAPH)) {
    st = write_sep(writer, writer->context.flags, SEP_GRAPH_END);
  }

  // Free any lingering contexts in case there was an error
  while (writer->anon_stack_size > 0) {
    pop_context(writer);
  }

  free_context(writer);

  serd_block_dumper_flush(&writer->output);

  writer->indent  = 0;
  writer->context = WRITE_CONTEXT_NULL;
  writer->empty   = true;
  return st;
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

  SerdBlockDumper dumper = {NULL, NULL, 0u, 0u};
  if (serd_block_dumper_open(&dumper, output, block_size)) {
    return NULL;
  }

  const WriteContext context = WRITE_CONTEXT_NULL;
  SerdWriter*        writer  = (SerdWriter*)calloc(1, sizeof(SerdWriter));

  writer->world     = world;
  writer->syntax    = syntax;
  writer->flags     = flags;
  writer->env       = env;
  writer->root_node = NULL;
  writer->root_uri  = SERD_URI_NULL;
  writer->output    = dumper;
  writer->context   = context;
  writer->empty     = true;

  writer->anon_stack =
    (WriteContext*)calloc(anon_stack_capacity, sizeof(WriteContext));

  writer->iface.handle   = writer;
  writer->iface.on_event = (SerdEventFunc)serd_writer_on_event;

  return writer;
}

SerdStatus
serd_writer_set_base_uri(SerdWriter* writer, const SerdNode* uri)
{
  assert(writer);

  if (uri->type != SERD_URI) {
    return SERD_ERR_BAD_ARG;
  }

  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
    if (ctx(writer, SERD_GRAPH) || ctx(writer, SERD_SUBJECT)) {
      TRY(st, esink(" .\n\n", 4, writer));
      reset_context(writer, true);
    }

    TRY(st, esink("@base <", 7, writer));
    TRY(st, esink(serd_node_string(uri), uri->length, writer));
    TRY(st, esink("> .\n", 4, writer));
  }

  reset_context(writer, true);

  return st;
}

SerdStatus
serd_writer_set_root_uri(SerdWriter* writer, const SerdStringView uri)
{
  assert(writer);

  serd_node_free(writer->root_node);
  writer->root_node = NULL;
  writer->root_uri  = SERD_URI_NULL;

  if (uri.len) {
    writer->root_node = serd_new_uri(uri);
    writer->root_uri  = serd_node_uri_view(writer->root_node);
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri)
{
  if (name->type != SERD_LITERAL || uri->type != SERD_URI) {
    return SERD_ERR_BAD_ARG;
  }

  SerdStatus st = SERD_SUCCESS;

  if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
    if (ctx(writer, SERD_GRAPH) || ctx(writer, SERD_SUBJECT)) {
      TRY(st, esink(" .\n\n", 4, writer));
      reset_context(writer, true);
    }

    TRY(st, esink("@prefix ", 8, writer));
    TRY(st, esink(serd_node_string(name), name->length, writer));
    TRY(st, esink(": <", 3, writer));
    TRY(st, write_uri_from_node(writer, uri));
    TRY(st, esink("> .\n", 4, writer));
  }

  reset_context(writer, true);
  return st;
}

void
serd_writer_free(SerdWriter* writer)
{
  if (!writer) {
    return;
  }

  serd_writer_finish(writer);
  serd_block_dumper_close(&writer->output);
  free(writer->anon_stack);
  serd_node_free(writer->root_node);
  free(writer);
}

const SerdSink*
serd_writer_sink(SerdWriter* writer)
{
  assert(writer);
  return &writer->iface;
}
