// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_ntriples.h"

#include "node.h"
#include "node_impl.h"
#include "read_utf8.h"
#include "reader.h"
#include "stack.h"
#include "string_utils.h"
#include "try.h"
#include "uri_utils.h"

#include "serd/event.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/syntax.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Utilities

static inline bool
codepoint_in_range(const uint32_t c, const uint32_t min, const uint32_t max)
{
  return c >= min && c <= max;
}

bool
is_PN_CHARS_BASE(const uint32_t c)
{
  return (codepoint_in_range(c, 'A', 'Z') || codepoint_in_range(c, 'a', 'z') ||
          codepoint_in_range(c, 0x000C0U, 0x000D6U) ||
          codepoint_in_range(c, 0x000D8U, 0x000F6U) ||
          codepoint_in_range(c, 0x000F8U, 0x002FFU) ||
          codepoint_in_range(c, 0x00370U, 0x0037DU) ||
          codepoint_in_range(c, 0x0037FU, 0x01FFFU) ||
          codepoint_in_range(c, 0x0200CU, 0x0200DU) ||
          codepoint_in_range(c, 0x02070U, 0x0218FU) ||
          codepoint_in_range(c, 0x02C00U, 0x02FEFU) ||
          codepoint_in_range(c, 0x03001U, 0x0D7FFU) ||
          codepoint_in_range(c, 0x0F900U, 0x0FDCFU) ||
          codepoint_in_range(c, 0x0FDF0U, 0x0FFFDU) ||
          codepoint_in_range(c, 0x10000U, 0xEFFFFU));
}

// Terminals

/// [144s] LANGTAG
SerdStatus
read_LANGTAG(SerdReader* const reader, SerdNode** const dest)
{
  int c = peek_byte(reader);
  if (!is_alpha(c)) {
    return r_err(reader, SERD_BAD_SYNTAX, "unexpected '%c'", c);
  }

  if (!(*dest = push_node(reader, SERD_LITERAL, "", 0))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st = SERD_SUCCESS;
  TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
  while ((c = peek_byte(reader)) && is_alpha(c)) {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
  }
  while (peek_byte(reader) == '-') {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, '-')));
    while ((c = peek_byte(reader)) && (is_alpha(c) || is_digit(c))) {
      TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    }
  }
  return SERD_SUCCESS;
}

static bool
is_EOL(const int c)
{
  return c == '\n' || c == '\r';
}

/// [7] EOL
SerdStatus
read_EOL(SerdReader* const reader)
{
  if (!is_EOL(peek_byte(reader))) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected a line ending");
  }

  while (is_EOL(peek_byte(reader))) {
    eat_byte(reader);
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_IRI_scheme(SerdReader* const reader, SerdNode* const dest)
{
  int c = peek_byte(reader);
  if (!is_alpha(c)) {
    return r_err(
      reader, SERD_BAD_SYNTAX, "'%c' is not a valid first IRI character", c);
  }

  SerdStatus st = SERD_SUCCESS;
  while (!st && (c = peek_byte(reader)) != EOF) {
    if (c == ':') {
      return SERD_SUCCESS; // End of scheme
    }

    st = is_uri_scheme_char(c)
           ? push_byte(reader, dest, eat_byte_safe(reader, c))
           : r_err(reader,
                   SERD_BAD_SYNTAX,
                   "U+%04X is not a valid IRI scheme character",
                   (unsigned)c);
  }

  return st ? st : SERD_BAD_SYNTAX;
}

SerdStatus
read_IRIREF_suffix(SerdReader* const reader, SerdNode* const node)
{
  SerdStatus st   = SERD_SUCCESS;
  uint32_t   code = 0U;

  while (st <= SERD_FAILURE) {
    const int c = eat_byte(reader);
    switch (c) {
    case EOF:
      return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");

    case ' ':
    case '"':
    case '<':
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      return r_err(
        reader, SERD_BAD_SYNTAX, "'%c' is not a valid IRI character", c);

    case '>':
      return SERD_SUCCESS;

    case '\\':
      TRY(st, read_UCHAR(reader, node, &code));

      if (!code || code == ' ' || code == '<' || code == '>') {
        return r_err(
          reader, SERD_BAD_SYNTAX, "U+%04X is not a valid IRI character", code);
      }

      break;

    default:
      if (c <= 0x20) {
        st = r_err(reader,
                   SERD_BAD_SYNTAX,
                   "control character U+%04X is not a valid IRI character",
                   (uint32_t)c);

        if (reader->strict) {
          return st;
        }
      }

      st = ((uint8_t)c & 0x80)
             ? read_utf8_continuation(reader, node, (uint8_t)c)
             : push_byte(reader, node, c);
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

/**
   Read an absolute IRI.

   This is a stricter subset of [8] IRIREF in the NTriples grammar, since a
   scheme is required.  Handling this in the parser results in better error
   messages.
*/
static SerdStatus
read_IRI(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, eat_byte_check(reader, '<'));

  if (!(*dest = push_node(reader, SERD_URI, "", 0))) {
    return SERD_BAD_STACK;
  }

  if ((st = read_IRI_scheme(reader, *dest))) {
    return r_err(reader, st, "expected IRI scheme");
  }

  return read_IRIREF_suffix(reader, *dest);
}

SerdStatus
read_character(SerdReader* const reader, SerdNode* const dest, const uint8_t c)
{
  if (!(c & 0x80)) {
    if (c == 0xA || c == 0xD) {
      dest->flags |= SERD_HAS_NEWLINE;
    } else if (c == '"' || c == '\'') {
      dest->flags |= SERD_HAS_QUOTE;
    }

    return push_byte(reader, dest, c);
  }

  return read_utf8_continuation(reader, dest, c);
}

SerdStatus
read_string_escape(SerdReader* const reader, SerdNode* const ref)
{
  SerdStatus st   = SERD_SUCCESS;
  uint32_t   code = 0;
  if ((st = read_ECHAR(reader, ref)) && (st = read_UCHAR(reader, ref, &code))) {
    return r_err(reader, st, "expected string escape sequence");
  }

  return st;
}

SerdStatus
read_STRING_LITERAL(SerdReader* const reader,
                    SerdNode* const   ref,
                    const uint8_t     q)
{
  SerdStatus st = SERD_SUCCESS;

  while (tolerate_status(reader, st)) {
    const int c = peek_byte(reader);
    switch (c) {
    case EOF:
      return r_err(reader, SERD_BAD_SYNTAX, "end of file in short string");
    case '\n':
    case '\r':
      return r_err(reader, SERD_BAD_SYNTAX, "line end in short string");
    case '\\':
      skip_byte(reader, c);
      TRY(st, read_string_escape(reader, ref));
      break;
    default:
      if (c == q) {
        return skip_byte(reader, c);
      }

      st = read_character(reader, ref, (uint8_t)eat_byte_safe(reader, c));
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

SerdStatus
read_PN_CHARS_BASE(SerdReader* const reader, SerdNode* const dest)
{
  uint32_t   code = 0;
  const int  c    = peek_byte(reader);
  SerdStatus st   = SERD_SUCCESS;

  if (is_alpha(c)) {
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  }

  if (c == EOF || !(c & 0x80)) {
    return SERD_FAILURE;
  }

  TRY(st, read_utf8_code_point(reader, dest, &code, (uint8_t)c));

  if (!is_PN_CHARS_BASE(code)) {
    r_err(
      reader, SERD_BAD_SYNTAX, "U+%04X is not a valid name character", code);
    if (reader->strict) {
      return SERD_BAD_SYNTAX;
    }
  }

  return st;
}

/**
   Read an initial prefixed name character.

   RDF 1.1 NTriples: [158s] PN_CHARS_U
*/
static SerdStatus
read_PN_CHARS_U(SerdReader* const reader, SerdNode* const dest)
{
  const int c = peek_byte(reader);

  return (c == ':' || c == '_')
           ? push_byte(reader, dest, eat_byte_safe(reader, c))
           : read_PN_CHARS_BASE(reader, dest);
}

SerdStatus
read_PN_CHARS(SerdReader* const reader, SerdNode* const dest)
{
  const int  c  = peek_byte(reader);
  SerdStatus st = SERD_SUCCESS;

  if (c == EOF) {
    return SERD_NO_DATA;
  }

  if (is_alpha(c) || is_digit(c) || c == '_' || c == '-') {
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  }

  if (!(c & 0x80)) {
    return SERD_FAILURE;
  }

  uint32_t code = 0U;
  TRY(st, read_utf8_code_point(reader, dest, &code, (uint8_t)c));

  if (!is_PN_CHARS_BASE(code) && code != 0xB7 &&
      !(code >= 0x0300 && code <= 0x036F) &&
      !(code >= 0x203F && code <= 0x2040)) {
    return r_err(
      reader, SERD_BAD_SYNTAX, "U+%04X is not a valid name character", code);
  }

  return st;
}

static bool
avoid_blank_clashes(const SerdReader* const reader)
{
  return reader->syntax == SERD_TURTLE || reader->syntax == SERD_TRIG;
}

static SerdStatus
adjust_blank_id(SerdReader* const reader, char* const buf)
{
  if (avoid_blank_clashes(reader) && is_digit(buf[reader->bprefix_len + 1])) {
    const char tag = buf[reader->bprefix_len];
    if (tag == 'b') {
      buf[reader->bprefix_len] = 'B'; // Prevent clash
      reader->seen_genid       = true;
    } else if (tag == 'B' && reader->seen_genid) {
      return r_err(reader,
                   SERD_BAD_LABEL,
                   "found both 'b' and 'B' blank IDs, prefix required");
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
read_BLANK_NODE_LABEL(SerdReader* const reader,
                      SerdNode** const  dest,
                      bool* const       ate_dot)
{
  SerdStatus st = SERD_SUCCESS;

  skip_byte(reader, '_');
  TRY(st, eat_byte_check(reader, ':'));

  int c = peek_byte(reader);
  if (c == EOF || c == ':') {
    // The spec says PN_CHARS_U, the tests say no colon, so exclude it here
    return r_err(reader, SERD_BAD_SYNTAX, "expected blank node label");
  }

  if (!(*dest = push_node(reader,
                          SERD_BLANK,
                          reader->bprefix ? reader->bprefix : "",
                          reader->bprefix_len))) {
    return SERD_BAD_STACK;
  }

  // Read first: (PN_CHARS_U | [0-9])
  SerdNode* const n = *dest;
  if (is_digit(c)) {
    TRY(st, push_byte(reader, n, eat_byte_safe(reader, c)));
  } else {
    TRY(st, read_PN_CHARS_U(reader, *dest));
  }

  // Read middle: (PN_CHARS | '.')*
  while (!st && (c = peek_byte(reader)) > 0) {
    st = (c == '.') ? push_byte(reader, n, eat_byte_safe(reader, c))
                    : read_PN_CHARS(reader, n);
  }

  if (st > SERD_FAILURE) {
    return st;
  }

  // Deal with annoying edge case of having eaten the trailing dot
  char* const buf = serd_node_buffer(n);
  if (buf[n->length - 1] == '.' && read_PN_CHARS(reader, n)) {
    --n->length;
    serd_stack_pop(&reader->stack, 1);
    *ate_dot = true;
  }

  // Adjust ID to avoid clashes with generated IDs if necessary
  st = adjust_blank_id(reader, buf);

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

static unsigned
utf8_from_codepoint(uint8_t* const out, const uint32_t code)
{
  const unsigned size = utf8_num_bytes_for_codepoint(code);
  uint32_t       c    = code;

  assert(size <= 4U);

  if (size == 4U) {
    out[3] = (uint8_t)(0x80U | (c & 0x3FU));
    c >>= 6U;
    c |= 0x10000U;
  }

  if (size >= 3U) {
    out[2] = (uint8_t)(0x80U | (c & 0x3FU));
    c >>= 6U;
    c |= 0x800U;
  }

  if (size >= 2U) {
    out[1] = (uint8_t)(0x80U | (c & 0x3FU));
    c >>= 6U;
    c |= 0xC0U;
  }

  if (size >= 1U) {
    out[0] = (uint8_t)c;
  }

  return size;
}

SerdStatus
read_UCHAR(SerdReader* const reader,
           SerdNode* const   node,
           uint32_t* const   code_point)
{
  SerdStatus st = SERD_SUCCESS;

  // Consume first character to determine which type of escape this is
  const int b      = peek_byte(reader);
  unsigned  length = 0U;
  if (b == 'U') {
    length = 8;
  } else if (b == 'u') {
    length = 4;
  } else {
    return r_err(reader, SERD_BAD_SYNTAX, "expected 'U' or 'u'");
  }

  TRY(st, skip_byte(reader, b));

  // Read character code point in hex
  uint8_t  buf[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t code   = 0U;
  for (unsigned i = 0; i < length; ++i) {
    if (!(buf[i] = read_HEX(reader))) {
      return SERD_BAD_SYNTAX;
    }

    code = (code << (i ? 4U : 0U)) | hex_digit_value(buf[i]);
  }

  // Reuse buf to write the UTF-8
  const unsigned size = utf8_from_codepoint(buf, code);
  if (!size) {
    *code_point = 0xFFFD;
    return (reader->strict
              ? r_err(reader, SERD_BAD_SYNTAX, "U+%X is out of range", code)
              : push_bytes(reader, node, replacement_char, 3));
  }

  *code_point = code;
  return push_bytes(reader, node, buf, size);
}

SerdStatus
read_ECHAR(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st = SERD_SUCCESS;
  const int  c  = peek_byte(reader);
  switch (c) {
  case 't':
    return (st = skip_byte(reader, 't')) ? st : push_byte(reader, dest, '\t');
  case 'b':
    return (st = skip_byte(reader, 'b')) ? st : push_byte(reader, dest, '\b');
  case 'n':
    dest->flags |= SERD_HAS_NEWLINE;
    return (st = skip_byte(reader, 'n')) ? st : push_byte(reader, dest, '\n');
  case 'r':
    dest->flags |= SERD_HAS_NEWLINE;
    return (st = skip_byte(reader, 'r')) ? st : push_byte(reader, dest, '\r');
  case 'f':
    return (st = skip_byte(reader, 'f')) ? st : push_byte(reader, dest, '\f');
  case '\\':
  case '"':
  case '\'':
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  default:
    return SERD_BAD_SYNTAX;
  }
}

uint8_t
read_HEX(SerdReader* const reader)
{
  const int c = peek_byte(reader);
  if (is_xdigit(c)) {
    return (uint8_t)eat_byte_safe(reader, c);
  }

  r_err(reader, SERD_BAD_SYNTAX, "invalid hexadecimal digit '%c'", c);
  return 0;
}

// Nonterminals

// comment ::= '#' ( [^#xA #xD] )*
SerdStatus
read_comment(SerdReader* const reader)
{
  skip_byte(reader, '#');

  for (int c = peek_byte(reader); c && c != '\n' && c != '\r' && c != EOF;) {
    skip_byte(reader, c);
    c = peek_byte(reader);
  }

  return SERD_SUCCESS;
}

/// [6] literal
static SerdStatus
read_literal(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st = SERD_SUCCESS;

  if (!(*dest = push_node(reader, SERD_LITERAL, "", 0))) {
    return SERD_BAD_STACK;
  }

  skip_byte(reader, '"');
  TRY(st, read_STRING_LITERAL(reader, *dest, '"'));

  SerdNode* datatype = NULL;
  SerdNode* lang     = NULL;
  const int next     = peek_byte(reader);
  if (next == '@') {
    TRY(st, skip_byte(reader, '@'));
    TRY(st, read_LANGTAG(reader, &lang));
    (*dest)->meta = lang;
    (*dest)->flags |= SERD_HAS_LANGUAGE;
  } else if (next == '^') {
    TRY(st, skip_byte(reader, '^'));
    TRY(st, eat_byte_check(reader, '^'));
    TRY(st, read_IRI(reader, &datatype));
    (*dest)->meta = datatype;
    (*dest)->flags |= SERD_HAS_DATATYPE;
  }

  return SERD_SUCCESS;
}

/// [3] subject
SerdStatus
read_nt_subject(SerdReader* const reader,
                SerdNode** const  dest,
                bool* const       ate_dot)
{
  const int c = peek_byte(reader);

  return (c == '<')   ? read_IRI(reader, dest)
         : (c == '_') ? read_BLANK_NODE_LABEL(reader, dest, ate_dot)
                      : r_err(reader, SERD_BAD_SYNTAX, "expected '<' or '_'");
}

/// [4] predicate
SerdStatus
read_nt_predicate(SerdReader* const reader, SerdNode** const dest)
{
  return read_IRI(reader, dest);
}

/// [4] object
SerdStatus
read_nt_object(SerdReader* const reader,
               SerdNode** const  dest,
               bool* const       ate_dot)
{
  *ate_dot = false;

  const int c = peek_byte(reader);

  return (c == '"')   ? read_literal(reader, dest)
         : (c == '<') ? read_IRI(reader, dest)
         : (c == '_')
           ? read_BLANK_NODE_LABEL(reader, dest, ate_dot)
           : r_err(reader, SERD_BAD_SYNTAX, "expected '<', '_', or '\"'");
}

/// [2] triple
static SerdStatus
read_triple(SerdReader* const reader)
{
  SerdStatementEventFlags flags   = 0U;
  ReadContext             ctx     = {0, 0, 0, 0, &flags};
  SerdStatus              st      = SERD_SUCCESS;
  bool                    ate_dot = false;

  // Read subject and predicate
  if ((st = read_nt_subject(reader, &ctx.subject, &ate_dot)) ||
      (st = skip_horizontal_whitespace(reader)) ||
      (st = read_nt_predicate(reader, &ctx.predicate)) ||
      (st = skip_horizontal_whitespace(reader))) {
    return st;
  }

  if ((st = read_nt_object(reader, &ctx.object, &ate_dot)) ||
      (st = skip_horizontal_whitespace(reader))) {
    return st;
  }

  if (!ate_dot && (st = eat_byte_check(reader, '.'))) {
    return st;
  }

  if (ctx.object) {
    TRY(st, push_node_termination(reader));
  }

  const SerdStatementView statement = {
    ctx.subject, ctx.predicate, ctx.object, ctx.graph};

  return serd_sink_write_statement(reader->sink, *ctx.flags, statement);
}

static SerdStatus
read_line(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, skip_horizontal_whitespace(reader));

  const int c = peek_byte(reader);
  if (c < 0) {
    return SERD_FAILURE;
  }

  if (c == 0) {
    skip_byte(reader, '\0');
    return SERD_FAILURE;
  }

  if (c == '\n' || c == '\r') {
    return read_EOL(reader);
  }

  if (c == '#') {
    return read_comment(reader);
  }

  const size_t orig_stack_size = reader->stack.size;

  if (!(st = read_triple(reader)) &&
      !(st = skip_horizontal_whitespace(reader))) {
    if (peek_byte(reader) == '#') {
      st = read_comment(reader);
    }
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);

  return (st || peek_byte(reader) < 0) ? st : read_EOL(reader);
}

/// [1] ntriplesDoc
SerdStatus
read_ntriplesDoc(SerdReader* const reader)
{
  // Read the first line
  SerdStatus st = read_line(reader);
  if (st == SERD_FAILURE || !tolerate_status(reader, st)) {
    return st;
  }

  // Continue reading lines for as long as possible
  for (st = SERD_SUCCESS; !st;) {
    st = read_line(reader);
    if (st > SERD_FAILURE && !reader->strict && tolerate_status(reader, st)) {
      serd_reader_skip_until_byte(reader, '\n');
      st = SERD_SUCCESS;
    }
  }

  // If we made it this far, we succeeded at reading at least one line
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}
