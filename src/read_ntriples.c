/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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

#include "read_ntriples.h"

#include "byte_source.h"
#include "caret.h"
#include "node.h"
#include "read_utf8.h"
#include "reader.h"
#include "stack.h"
#include "statement.h"
#include "string_utils.h"
#include "try.h"
#include "uri_utils.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Terminals

/// [144s] LANGTAG
SerdStatus
read_LANGTAG(SerdReader* const reader)
{
  if (!is_alpha(peek_byte(reader))) {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected A-Z or a-z");
  }

  SerdNode* const node = push_node(reader, SERD_LITERAL, "", 0);
  if (!node) {
    return SERD_ERR_OVERFLOW;
  }

  // First component must be all letters
  SerdStatus st = SERD_SUCCESS;
  TRY(st, push_byte(reader, node, eat_byte(reader)));
  while (is_alpha(peek_byte(reader))) {
    TRY(st, push_byte(reader, node, eat_byte(reader)));
  }

  // Following components can have letters and digits
  while (peek_byte(reader) == '-') {
    TRY(st, push_byte(reader, node, eat_byte(reader)));
    while (is_alpha(peek_byte(reader)) || is_digit(peek_byte(reader))) {
      TRY(st, push_byte(reader, node, eat_byte(reader)));
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
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected a line ending");
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
    return r_err(reader,
                 SERD_ERR_BAD_SYNTAX,
                 "'%c' is not a valid first IRI character",
                 c);
  }

  SerdStatus st = SERD_SUCCESS;
  while (!st && (c = peek_byte(reader)) != EOF) {
    if (c == ':') {
      return SERD_SUCCESS; // End of scheme
    }

    st = is_uri_scheme_char(c)
           ? push_byte(reader, dest, eat_byte_safe(reader, c))
           : r_err(reader,
                   SERD_ERR_BAD_SYNTAX,
                   "U+%04X is not a valid IRI scheme character",
                   (unsigned)c);
  }

  return st ? st : SERD_ERR_BAD_SYNTAX;
}

SerdStatus
read_IRIREF_suffix(SerdReader* const reader, SerdNode* const node)
{
  SerdStatus st   = SERD_SUCCESS;
  uint32_t   code = 0u;

  while (st <= SERD_FAILURE) {
    const int c = eat_byte(reader);
    switch (c) {
    case EOF:
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected end of file");

    case ' ':
    case '"':
    case '<':
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      return r_err(
        reader, SERD_ERR_BAD_SYNTAX, "'%c' is not a valid IRI character", c);

    case '>':
      return SERD_SUCCESS;

    case '\\':
      if ((st = read_UCHAR(reader, node, &code))) {
        return st;
      }

      if (!code || code == ' ' || code == '<' || code == '>') {
        return r_err(reader,
                     SERD_ERR_BAD_SYNTAX,
                     "U+%04X is not a valid IRI character",
                     code);
      }

      break;

    default:
      if (c <= 0x20) {
        st = r_err(reader,
                   SERD_ERR_BAD_SYNTAX,
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

SerdStatus
read_IRI(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st = SERD_SUCCESS;
  if ((st = eat_byte_check(reader, '<'))) {
    return st;
  }

  if (!(*dest = push_node(reader, SERD_URI, "", 0))) {
    return SERD_ERR_OVERFLOW;
  }

  if ((st = read_IRI_scheme(reader, *dest))) {
    return r_err(reader, st, "expected IRI scheme");
  }

  return read_IRIREF_suffix(reader, *dest);
}

// Read one character (possibly multi-byte)
// The first byte, c, has already been eaten by caller
static SerdStatus
read_character(SerdReader* const reader, SerdNode* const dest, const uint8_t c)
{
  if (!(c & 0x80)) {
    switch (c) {
    case 0xA:
    case 0xD:
      dest->flags |= SERD_HAS_NEWLINE;
      break;
    case '"':
    case '\'':
      dest->flags |= SERD_HAS_QUOTE;
      break;
    default:
      break;
    }

    return push_byte(reader, dest, c);
  }

  return read_utf8_continuation(reader, dest, c);
}

/// [9]  STRING_LITERAL_QUOTE
/// [23] STRING_LITERAL_SINGLE_QUOTE
SerdStatus
read_STRING_LITERAL(SerdReader* const reader,
                    SerdNode* const   ref,
                    const uint8_t     q)
{
  SerdStatus st = SERD_SUCCESS;

  while (tolerate_status(reader, st)) {
    const int c    = peek_byte(reader);
    uint32_t  code = 0;
    switch (c) {
    case EOF:
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "end of file in short string");
    case '\n':
    case '\r':
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "line end in short string");
    case '\\':
      eat_byte_safe(reader, c);
      if ((st = read_ECHAR(reader, ref)) &&
          (st = read_UCHAR(reader, ref, &code))) {
        return r_err(reader, st, "invalid escape `\\%c'", peek_byte(reader));
      }
      break;
    default:
      eat_byte_safe(reader, c);
      if (c == q) {
        return SERD_SUCCESS;
      }

      st = read_character(reader, ref, (uint8_t)c);
    }
  }

  if (st && reader->strict) {
    r_err(reader, st, "failed to read literal (%s)", serd_strerror(st));
  }

  return st;
}

static SerdStatus
adjust_blank_id(SerdReader* const reader, char* const buf)
{
  if (!(reader->flags & SERD_READ_EXACT_BLANKS) &&
      is_digit(buf[reader->bprefix_len + 1])) {
    const char tag = buf[reader->bprefix_len];
    if (tag == 'b') {
      buf[reader->bprefix_len] = 'B'; // Prevent clash
      reader->seen_genid       = true;
    } else if (tag == 'B' && reader->seen_genid) {
      return r_err(reader,
                   SERD_ERR_ID_CLASH,
                   "found both `b' and `B' blank IDs, prefix required");
    }
  }

  return SERD_SUCCESS;
}

/// [141s] BLANK_NODE_LABEL
SerdStatus
read_BLANK_NODE_LABEL(SerdReader* const reader,
                      SerdNode** const  dest,
                      bool* const       ate_dot)
{
  SerdStatus st = SERD_SUCCESS;

  eat_byte_safe(reader, '_');
  if ((st = eat_byte_check(reader, ':'))) {
    return st;
  }

  if (!(*dest = push_node(reader,
                          SERD_BLANK,
                          reader->bprefix ? reader->bprefix : "",
                          reader->bprefix_len))) {
    return SERD_ERR_OVERFLOW;
  }

  // Read first: (PN_CHARS_U | [0-9])
  SerdNode* const n = *dest;
  int             c = peek_byte(reader);
  if (is_digit(c)) {
    TRY(st, push_byte(reader, n, eat_byte_safe(reader, c)));
  } else {
    TRY(st, read_PN_CHARS_U(reader, *dest));
  }

  // Read middle: (PN_CHARS | '.')*
  while ((c = peek_byte(reader))) {
    if (c == '.') {
      TRY(st, push_byte(reader, n, eat_byte_safe(reader, c)));
    } else if ((st = read_PN_CHARS(reader, n))) {
      break;
    }
  }

  // Deal with annoying edge case of having eaten the trailing dot
  char* const buf = serd_node_buffer(n);
  if (buf[n->length - 1] == '.' && read_PN_CHARS(reader, n)) {
    --n->length;
    serd_stack_pop(&reader->stack, 1);
    *ate_dot = true;
  }

  if (!tolerate_status(reader, st)) {
    return st;
  }

  // Adjust ID to avoid clashes with generated IDs if necessary
  return adjust_blank_id(reader, buf);
}

static unsigned
utf8_from_codepoint(uint8_t* const out, const uint32_t code)
{
  const unsigned size = utf8_num_bytes_for_codepoint(code);
  uint32_t       c    = code;

  if (!size || size > 4u) {
    return size;
  }

  if (size == 4u) {
    out[3] = (uint8_t)(0x80u | (c & 0x3Fu));
    c >>= 6;
    c |= 0x10000;
  }

  if (size >= 3u) {
    out[2] = (uint8_t)(0x80u | (c & 0x3Fu));
    c >>= 6;
    c |= 0x800;
  }

  if (size >= 2u) {
    out[1] = (uint8_t)(0x80u | (c & 0x3Fu));
    c >>= 6;
    c |= 0xC0;
  }

  if (size >= 1u) {
    out[0] = (uint8_t)c;
  }

  return size;
}

/// [10] UCHAR
SerdStatus
read_UCHAR(SerdReader* const reader,
           SerdNode* const   node,
           uint32_t* const   code_point)
{
  // Consume first character to determine which type of escape this is
  const int b      = peek_byte(reader);
  unsigned  length = 0u;
  switch (b) {
  case 'U':
    length = 8;
    break;
  case 'u':
    length = 4;
    break;
  default:
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected 'U' or 'u'");
  }
  eat_byte_safe(reader, b);

  // Read character code point in hex
  uint8_t buf[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  for (unsigned i = 0; i < length; ++i) {
    if (!(buf[i] = read_HEX(reader))) {
      return SERD_ERR_BAD_SYNTAX;
    }
  }

  // Parse code point from buf, then reuse buf to write the UTF-8
  char*          endptr = NULL;
  const uint32_t code   = (uint32_t)strtoul((const char*)buf, &endptr, 16);
  const unsigned size   = utf8_from_codepoint(buf, code);

  if (!size) {
    *code_point = 0xFFFD;
    return (reader->strict
              ? r_err(reader, SERD_ERR_BAD_SYNTAX, "U+%X is out of range", code)
              : push_bytes(reader, node, replacement_char, 3));
  }

  *code_point = code;
  return push_bytes(reader, node, buf, size);
}

/// [153s] ECHAR
SerdStatus
read_ECHAR(SerdReader* const reader, SerdNode* const dest)
{
  const int c = peek_byte(reader);
  switch (c) {
  case 't':
    eat_byte_safe(reader, 't');
    return push_byte(reader, dest, '\t');
  case 'b':
    eat_byte_safe(reader, 'b');
    return push_byte(reader, dest, '\b');
  case 'n':
    dest->flags |= SERD_HAS_NEWLINE;
    eat_byte_safe(reader, 'n');
    return push_byte(reader, dest, '\n');
  case 'r':
    dest->flags |= SERD_HAS_NEWLINE;
    eat_byte_safe(reader, 'r');
    return push_byte(reader, dest, '\r');
  case 'f':
    eat_byte_safe(reader, 'f');
    return push_byte(reader, dest, '\f');
  case '\\':
  case '"':
  case '\'':
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  default:
    return SERD_ERR_BAD_SYNTAX;
  }
}

/// [157s] PN_CHARS_BASE
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

  if ((st = read_utf8_code_point(reader, dest, &code, (uint8_t)c))) {
    return st;
  }

  if (!is_PN_CHARS_BASE(code)) {
    r_err(reader,
          SERD_ERR_BAD_SYNTAX,
          "U+%04X is not a valid name character",
          code);
    if (reader->strict) {
      return SERD_ERR_BAD_SYNTAX;
    }
  }

  return st;
}

/// [158s] PN_CHARS_U
SerdStatus
read_PN_CHARS_U(SerdReader* const reader, SerdNode* const dest)
{
  const int c = peek_byte(reader);

  switch (c) {
  case ':':
  case '_':
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  default:
    break;
  }

  return read_PN_CHARS_BASE(reader, dest);
}

// [160s] PN_CHARS
SerdStatus
read_PN_CHARS(SerdReader* const reader, SerdNode* const dest)
{
  const int  c  = peek_byte(reader);
  SerdStatus st = SERD_SUCCESS;

  if (c == EOF) {
    return SERD_ERR_NO_DATA;
  }

  if (is_alpha(c) || is_digit(c) || c == '_' || c == '-') {
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  }

  if (!(c & 0x80)) {
    return SERD_FAILURE;
  }

  uint32_t code = 0u;
  if ((st = read_utf8_code_point(reader, dest, &code, (uint8_t)c))) {
    return st;
  }

  if (!is_PN_CHARS_BASE(code) && code != 0xB7 &&
      !(code >= 0x0300 && code <= 0x036F) &&
      !(code >= 0x203F && code <= 0x2040)) {
    return r_err(reader,
                 SERD_ERR_BAD_SYNTAX,
                 "U+%04X is not a valid name character",
                 code);
  }

  return st;
}

/// [162s] HEX
uint8_t
read_HEX(SerdReader* const reader)
{
  const int c = peek_byte(reader);
  if (is_xdigit(c)) {
    return (uint8_t)eat_byte_safe(reader, c);
  }

  r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid hexadecimal digit `%c'", c);
  return 0;
}

SerdStatus
read_VARNAME(SerdReader* const reader, SerdNode** const dest)
{
  // Simplified from SPARQL: VARNAME ::= (PN_CHARS_U | [0-9])+
  SerdNode*  n  = *dest;
  SerdStatus st = SERD_SUCCESS;
  int        c  = 0;
  peek_byte(reader);
  while ((c = peek_byte(reader))) {
    if (is_digit(c) || c == '_') {
      st = push_byte(reader, n, eat_byte_safe(reader, c));
    } else if ((st = read_PN_CHARS(reader, n))) {
      st = st > SERD_FAILURE ? st : SERD_SUCCESS;
      break;
    }
  }

  return st;
}

SerdStatus
read_Var(SerdReader* const reader, SerdNode** const dest)
{
  assert(peek_byte(reader) == '$' || peek_byte(reader) == '?');

  if (!(reader->flags & SERD_READ_VARIABLES)) {
    return r_err(
      reader, SERD_ERR_BAD_SYNTAX, "syntax does not support variables");
  }

  if (!(*dest = push_node(reader, SERD_VARIABLE, "", 0))) {
    return SERD_ERR_OVERFLOW;
  }

  serd_byte_source_advance(reader->source);

  return read_VARNAME(reader, dest);
}

// Nonterminals

// comment ::= '#' ( [^#xA #xD] )*
SerdStatus
read_comment(SerdReader* const reader)
{
  eat_byte_safe(reader, '#');

  for (int c = peek_byte(reader); c && c != '\n' && c != '\r' && c != EOF;) {
    eat_byte_safe(reader, c);
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
    return SERD_ERR_OVERFLOW;
  }

  eat_byte_safe(reader, '"');
  if ((st = read_STRING_LITERAL(reader, *dest, '"'))) {
    return st;
  }

  SerdNode* datatype = NULL;
  switch (peek_byte(reader)) {
  case '@':
    eat_byte_safe(reader, '@');
    (*dest)->flags |= SERD_HAS_LANGUAGE;
    TRY(st, read_LANGTAG(reader));
    break;
  case '^':
    eat_byte_safe(reader, '^');
    if (!(st = eat_byte_check(reader, '^'))) {
      (*dest)->flags |= SERD_HAS_DATATYPE;
      TRY(st, read_IRI(reader, &datatype));
    }
    break;
  }

  return st;
}

/// [3] subject
SerdStatus
read_nt_subject(SerdReader* const reader, SerdNode** const dest)
{
  bool ate_dot = false;

  switch (peek_byte(reader)) {
  case '<':
    return read_IRI(reader, dest);

  case '?':
    return read_Var(reader, dest);

  case '_':
    return read_BLANK_NODE_LABEL(reader, dest, &ate_dot);

  default:
    break;
  }

  return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected '<' or '_'");
}

/// [4] predicate
SerdStatus
read_nt_predicate(SerdReader* const reader, SerdNode** const dest)
{
  return (peek_byte(reader) == '?') ? read_Var(reader, dest)
                                    : read_IRI(reader, dest);
}

/// [4] object
SerdStatus
read_nt_object(SerdReader* const reader,
               SerdNode** const  dest,
               bool* const       ate_dot)
{
  *ate_dot = false;

  switch (peek_byte(reader)) {
  case '"':
    return read_literal(reader, dest);

  case '<':
    return read_IRI(reader, dest);

  case '?':
    return read_Var(reader, dest);

  case '_':
    return read_BLANK_NODE_LABEL(reader, dest, ate_dot);

  default:
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected '<', '_', or '\"'");
  }
}

/// [2] triple
static SerdStatus
read_triple(SerdReader* const reader)
{
  SerdStatementFlags flags   = 0;
  ReadContext        ctx     = {0, 0, 0, 0, &flags};
  SerdStatus         st      = SERD_SUCCESS;
  bool               ate_dot = false;

  // Read subject and predicate
  if ((st = read_nt_subject(reader, &ctx.subject)) ||
      (st = skip_horizontal_whitespace(reader)) ||
      (st = read_nt_predicate(reader, &ctx.predicate)) ||
      (st = skip_horizontal_whitespace(reader))) {
    if (!reader->strict) {
      skip_until(reader, '\n');
    }

    return st;
  }

  // Preserve the caret for error reporting and read object
  SerdCaret orig_caret = reader->source->caret;
  if ((st = read_nt_object(reader, &ctx.object, &ate_dot)) ||
      (st = skip_horizontal_whitespace(reader))) {
    if (!reader->strict) {
      skip_until(reader, '\n');
    }

    return st;
  }

  if (!ate_dot && (st = eat_byte_check(reader, '.'))) {
    return st;
  }

  if (ctx.object) {
    serd_node_zero_pad(ctx.object);
  }

  const SerdStatement statement = {
    {ctx.subject, ctx.predicate, ctx.object, ctx.graph}, &orig_caret};

  return serd_sink_write_statement(reader->sink, *ctx.flags, &statement);
}

static SerdStatus
read_line(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;

  skip_horizontal_whitespace(reader);

  switch (peek_byte(reader)) {
  case EOF:
    return SERD_FAILURE;

  case '\n':
  case '\r':
    return read_EOL(reader);

  case '#':
    st = read_comment(reader);
    break;

  default:
    if (!(st = read_triple(reader))) {
      skip_horizontal_whitespace(reader);
      if (peek_byte(reader) == '#') {
        st = read_comment(reader);
      }
    }
    break;
  }

  return (st || peek_byte(reader) == EOF) ? st : read_EOL(reader);
}

/// [1] ntriplesDoc
SerdStatus
read_ntriplesDoc(SerdReader* const reader)
{
  // Record the initial stack size and read the first line
  const size_t orig_stack_size = reader->stack.size;
  SerdStatus   st              = read_line(reader);

  // Return if the first line didn't read for any reason
  serd_stack_pop_to(&reader->stack, orig_stack_size);
  if (st) {
    return st;
  }

  // Continue reading lines for as long as possible
  while (!(st = read_line(reader))) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);
  }

  // If we made it this far, we succeeded at reading at least one line
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}
