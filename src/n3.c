// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"
#include "namespaces.h"
#include "node.h"
#include "reader.h"
#include "stack.h"
#include "string_utils.h"
#include "try.h"
#include "uri_utils.h"

#include "serd/byte_source.h"
#include "serd/env.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/syntax.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__) && __clang_major__ >= 10
#  define SERD_FALLTHROUGH __attribute__((fallthrough))
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wmissing-declarations\"")
#elif defined(__GNUC__) && __GNUC__ >= 7
#  define SERD_FALLTHROUGH __attribute__((fallthrough))
#else
#  define SERD_FALLTHROUGH
#endif

static bool
fancy_syntax(const SerdReader* const reader)
{
  return reader->syntax == SERD_TURTLE || reader->syntax == SERD_TRIG;
}

static SerdStatus
read_collection(SerdReader* reader, ReadContext ctx, SerdNode** dest);

static SerdStatus
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot);

static uint8_t
read_HEX(SerdReader* const reader)
{
  const int c = peek_byte(reader);
  if (is_xdigit(c)) {
    return (uint8_t)eat_byte_safe(reader, c);
  }

  r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid hexadecimal digit '%c'", c);
  return 0;
}

// Read UCHAR escape, initial \ is already eaten by caller
static SerdStatus
read_UCHAR(SerdReader* const reader,
           SerdNode* const   dest,
           uint32_t* const   char_code)
{
  const int b      = peek_byte(reader);
  unsigned  length = 0;
  switch (b) {
  case 'U':
    length = 8;
    break;
  case 'u':
    length = 4;
    break;
  default:
    return SERD_ERR_BAD_SYNTAX;
  }

  skip_byte(reader, b);

  uint8_t buf[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  for (unsigned i = 0; i < length; ++i) {
    if (!(buf[i] = read_HEX(reader))) {
      return SERD_ERR_BAD_SYNTAX;
    }
  }

  char*          endptr = NULL;
  const uint32_t code   = (uint32_t)strtoul((const char*)buf, &endptr, 16);
  assert(endptr == (char*)buf + length);

  unsigned size = 0;
  if (code < 0x00000080) {
    size = 1;
  } else if (code < 0x00000800) {
    size = 2;
  } else if (code < 0x00010000) {
    size = 3;
  } else if (code < 0x00110000) {
    size = 4;
  } else {
    r_err(
      reader, SERD_ERR_BAD_SYNTAX, "unicode character 0x%X out of range", code);

    *char_code          = 0xFFFD;
    const SerdStatus st = push_bytes(reader, dest, replacement_char, 3);
    return st ? st : SERD_SUCCESS;
  }

  // Build output in buf
  // (Note # of bytes = # of leading 1 bits in first byte)
  uint32_t c = code;
  switch (size) {
  case 4:
    buf[3] = (uint8_t)(0x80U | (c & 0x3FU));
    c >>= 6;
    c |= (16 << 12); // set bit 4
    SERD_FALLTHROUGH;
  case 3:
    buf[2] = (uint8_t)(0x80U | (c & 0x3FU));
    c >>= 6;
    c |= (32 << 6); // set bit 5
    SERD_FALLTHROUGH;
  case 2:
    buf[1] = (uint8_t)(0x80U | (c & 0x3FU));
    c >>= 6;
    c |= 0xC0; // set bits 6 and 7
    SERD_FALLTHROUGH;
  case 1:
    buf[0] = (uint8_t)c;
    SERD_FALLTHROUGH;
  default:
    break;
  }

  *char_code = code;
  return push_bytes(reader, dest, buf, size);
}

// Read ECHAR escape, initial \ is already eaten by caller
static SerdStatus
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
    return SERD_ERR_BAD_SYNTAX;
  }
}

static SerdStatus
bad_char(SerdReader* const reader, const char* const fmt, const uint8_t c)
{
  // Skip bytes until the next start byte
  for (int b = peek_byte(reader); b != EOF && ((uint8_t)b & 0x80);) {
    skip_byte(reader, b);
    b = peek_byte(reader);
  }

  r_err(reader, SERD_ERR_BAD_SYNTAX, fmt, c);
  return reader->strict ? SERD_ERR_BAD_SYNTAX : SERD_FAILURE;
}

static SerdStatus
read_utf8_bytes(SerdReader* const reader,
                uint8_t           bytes[4],
                uint32_t* const   size,
                const uint8_t     c)
{
  *size = utf8_num_bytes(c);
  if (*size <= 1 || *size > 4) {
    return bad_char(reader, "invalid UTF-8 start 0x%X", c);
  }

  bytes[0] = c;
  for (unsigned i = 1; i < *size; ++i) {
    const int b = peek_byte(reader);
    if (b == EOF || ((uint8_t)b & 0x80) == 0) {
      return bad_char(reader, "invalid UTF-8 continuation 0x%X", (uint8_t)b);
    }

    bytes[i] = (uint8_t)eat_byte_safe(reader, b);
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_utf8_character(SerdReader* const reader,
                    SerdNode* const   dest,
                    const uint8_t     c)
{
  uint32_t   size     = 0;
  uint8_t    bytes[4] = {0, 0, 0, 0};
  SerdStatus st       = read_utf8_bytes(reader, bytes, &size, c);

  if (!tolerate_status(reader, st)) {
    return st;
  }

  if (st) {
    const SerdStatus rst = push_bytes(reader, dest, replacement_char, 3);
    return rst ? rst : st;
  }

  return push_bytes(reader, dest, bytes, size);
}

static SerdStatus
read_utf8_code(SerdReader* const reader,
               SerdNode* const   dest,
               uint32_t* const   code,
               const uint8_t     c)
{
  uint32_t   size     = 0;
  uint8_t    bytes[4] = {0, 0, 0, 0};
  SerdStatus st       = read_utf8_bytes(reader, bytes, &size, c);
  if (st) {
    const SerdStatus rst = push_bytes(reader, dest, replacement_char, 3);
    return rst ? rst : st;
  }

  if (!(st = push_bytes(reader, dest, bytes, size))) {
    *code = parse_counted_utf8_char(bytes, size);
  }

  return st;
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
  return read_utf8_character(reader, dest, c);
}

// [10] comment ::= '#' ( [^#xA #xD] )*
static void
read_comment(SerdReader* const reader)
{
  skip_byte(reader, '#');

  int c = 0;
  while (((c = peek_byte(reader)) != 0xA) && c != 0xD && c != EOF && c) {
    skip_byte(reader, c);
  }
}

// [24] ws ::= #x9 | #xA | #xD | #x20 | comment
static bool
read_ws(SerdReader* const reader)
{
  const int c = peek_byte(reader);
  switch (c) {
  case 0x9:
  case 0xA:
  case 0xD:
  case 0x20:
    skip_byte(reader, c);
    return true;
  case '#':
    read_comment(reader);
    return true;
  default:
    return false;
  }
}

static bool
read_ws_star(SerdReader* const reader)
{
  while (read_ws(reader)) {
  }

  return true;
}

static bool
peek_delim(SerdReader* const reader, const uint8_t delim)
{
  read_ws_star(reader);
  return peek_byte(reader) == delim;
}

static bool
eat_delim(SerdReader* const reader, const uint8_t delim)
{
  if (peek_delim(reader, delim)) {
    skip_byte(reader, delim);
    return read_ws_star(reader);
  }

  return false;
}

// STRING_LITERAL_LONG_QUOTE and STRING_LITERAL_LONG_SINGLE_QUOTE
// Initial triple quotes are already eaten by caller
static SerdStatus
read_STRING_LITERAL_LONG(SerdReader* const reader,
                         SerdNode* const   ref,
                         const uint8_t     q)
{
  SerdStatus st = SERD_SUCCESS;
  while (tolerate_status(reader, st)) {
    const int c = peek_byte(reader);
    if (c == '\\') {
      skip_byte(reader, c);
      uint32_t code = 0;
      if ((st = read_ECHAR(reader, ref)) &&
          (st = read_UCHAR(reader, ref, &code))) {
        return r_err(reader, st, "invalid escape '\\%c'", peek_byte(reader));
      }
    } else if (c == EOF) {
      st = r_err(reader, SERD_ERR_NO_DATA, "unexpected end of file");
    } else if (c == q) {
      skip_byte(reader, q);
      const int q2 = eat_byte_safe(reader, peek_byte(reader));
      const int q3 = peek_byte(reader);
      if (q2 == q && q3 == q) { // End of string
        skip_byte(reader, q3);
        break;
      }
      ref->flags |= SERD_HAS_QUOTE;
      if (!(st = push_byte(reader, ref, c))) {
        st = read_character(reader, ref, (uint8_t)q2);
      }
    } else {
      st = read_character(reader, ref, (uint8_t)eat_byte_safe(reader, c));
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

// STRING_LITERAL_QUOTE and STRING_LITERAL_SINGLE_QUOTE
// Initial quote is already eaten by caller
static SerdStatus
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
      skip_byte(reader, c);
      if ((st = read_ECHAR(reader, ref)) &&
          (st = read_UCHAR(reader, ref, &code))) {
        return r_err(reader, st, "invalid escape '\\%c'", peek_byte(reader));
      }
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

static SerdStatus
read_String(SerdReader* const reader, SerdNode* const node)
{
  const int q1 = eat_byte_safe(reader, peek_byte(reader));
  const int q2 = peek_byte(reader);
  if (q2 == EOF) {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected end of file");
  }

  if (q2 != q1) { // Short string (not triple quoted)
    return read_STRING_LITERAL(reader, node, (uint8_t)q1);
  }

  skip_byte(reader, q2);
  const int q3 = peek_byte(reader);
  if (q3 == EOF) {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected end of file");
  }

  if (q3 != q1) { // Empty short string ("" or '')
    return SERD_SUCCESS;
  }

  if (!fancy_syntax(reader)) {
    return r_err(
      reader, SERD_ERR_BAD_SYNTAX, "syntax does not support long literals");
  }

  skip_byte(reader, q3);
  return read_STRING_LITERAL_LONG(reader, node, (uint8_t)q1);
}

static bool
is_PN_CHARS_BASE(const uint32_t c)
{
  return ((c >= 0x00C0 && c <= 0x00D6) || (c >= 0x00D8 && c <= 0x00F6) ||
          (c >= 0x00F8 && c <= 0x02FF) || (c >= 0x0370 && c <= 0x037D) ||
          (c >= 0x037F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D) ||
          (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) ||
          (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) ||
          (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF));
}

static SerdStatus
read_PN_CHARS_BASE(SerdReader* const reader, SerdNode* const dest)
{
  uint32_t   code = 0;
  const int  c    = peek_byte(reader);
  SerdStatus st   = SERD_SUCCESS;
  if (is_alpha(c)) {
    st = push_byte(reader, dest, eat_byte_safe(reader, c));
  } else if (c == EOF || !(c & 0x80)) {
    return SERD_FAILURE;
  } else if ((st = read_utf8_code(
                reader, dest, &code, (uint8_t)eat_byte_safe(reader, c)))) {
    return st;
  } else if (!is_PN_CHARS_BASE(code)) {
    r_err(
      reader, SERD_ERR_BAD_SYNTAX, "invalid character U+%04X in name", code);
    if (reader->strict) {
      return SERD_ERR_BAD_SYNTAX;
    }
  }
  return st;
}

static bool
is_PN_CHARS(const uint32_t c)
{
  return (is_PN_CHARS_BASE(c) || c == 0xB7 || (c >= 0x0300 && c <= 0x036F) ||
          (c >= 0x203F && c <= 0x2040));
}

static SerdStatus
read_PN_CHARS(SerdReader* const reader, SerdNode* const dest)
{
  uint32_t   code = 0;
  const int  c    = peek_byte(reader);
  SerdStatus st   = SERD_SUCCESS;
  if (is_alpha(c) || is_digit(c) || c == '_' || c == '-') {
    st = push_byte(reader, dest, eat_byte_safe(reader, c));
  } else if (c == EOF || !(c & 0x80)) {
    return SERD_FAILURE;
  } else if ((st = read_utf8_code(
                reader, dest, &code, (uint8_t)eat_byte_safe(reader, c)))) {
    return st;
  } else if (!is_PN_CHARS(code)) {
    return r_err(
      reader, SERD_ERR_BAD_SYNTAX, "invalid character U+%04X in name", code);
  }
  return st;
}

static SerdStatus
read_PERCENT(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st = push_byte(reader, dest, eat_byte_safe(reader, '%'));
  if (st) {
    return st;
  }

  const uint8_t h1 = read_HEX(reader);
  const uint8_t h2 = read_HEX(reader);
  if (!h1 || !h2) {
    return SERD_ERR_BAD_SYNTAX;
  }

  if (!(st = push_byte(reader, dest, h1))) {
    st = push_byte(reader, dest, h2);
  }

  return st;
}

static SerdStatus
read_PN_LOCAL_ESC(SerdReader* const reader, SerdNode* const dest)
{
  skip_byte(reader, '\\');

  const int c = peek_byte(reader);
  switch (c) {
  case '!':
  case '#':
  case '$':
  case '%':
  case '&':
  case '\'':
  case '(':
  case ')':
  case '*':
  case '+':
  case ',':
  case '-':
  case '.':
  case '/':
  case ';':
  case '=':
  case '?':
  case '@':
  case '_':
  case '~':
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  default:
    break;
  }

  return r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid escape");
}

static SerdStatus
read_PLX(SerdReader* const reader, SerdNode* const dest)
{
  const int c = peek_byte(reader);
  switch (c) {
  case '%':
    return read_PERCENT(reader, dest);
  case '\\':
    return read_PN_LOCAL_ESC(reader, dest);
  default:
    return SERD_FAILURE;
  }
}

static SerdStatus
read_PN_LOCAL(SerdReader* const reader,
              SerdNode* const   dest,
              bool* const       ate_dot)
{
  int        c                      = peek_byte(reader);
  SerdStatus st                     = SERD_SUCCESS;
  bool       trailing_unescaped_dot = false;
  switch (c) {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case ':':
  case '_':
    st = push_byte(reader, dest, eat_byte_safe(reader, c));
    break;
  default:
    if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
      return r_err(reader, st, "bad escape");
    } else if (st != SERD_SUCCESS && read_PN_CHARS_BASE(reader, dest)) {
      return SERD_FAILURE;
    }
  }

  while ((c = peek_byte(reader))) { // Middle: (PN_CHARS | '.' | ':')*
    if (c == '.' || c == ':') {
      st = push_byte(reader, dest, eat_byte_safe(reader, c));
    } else if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
      return r_err(reader, st, "bad escape");
    } else if (st != SERD_SUCCESS && (st = read_PN_CHARS(reader, dest))) {
      break;
    }
    trailing_unescaped_dot = (c == '.');
  }

  if (trailing_unescaped_dot) {
    // Ate trailing dot, pop it from stack/node and inform caller
    --dest->length;
    serd_stack_pop(&reader->stack, 1);
    *ate_dot = true;
  }

  return (st > SERD_FAILURE) ? st : SERD_SUCCESS;
}

// Read the remainder of a PN_PREFIX after some initial characters
static SerdStatus
read_PN_PREFIX_tail(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st = SERD_SUCCESS;
  int        c  = 0;
  while ((c = peek_byte(reader))) { // Middle: (PN_CHARS | '.')*
    if (c == '.') {
      st = push_byte(reader, dest, eat_byte_safe(reader, c));
    } else if ((st = read_PN_CHARS(reader, dest))) {
      break;
    }
  }

  if (st <= SERD_FAILURE &&
      serd_node_string(dest)[serd_node_length(dest) - 1] == '.') {
    if ((st = read_PN_CHARS(reader, dest))) {
      return r_err(reader,
                   st > SERD_FAILURE ? st : SERD_ERR_BAD_SYNTAX,
                   "prefix ends with '.'");
    }
  }

  return st;
}

static SerdStatus
read_PN_PREFIX(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st = SERD_SUCCESS;

  if (!(st = read_PN_CHARS_BASE(reader, dest))) {
    return read_PN_PREFIX_tail(reader, dest);
  }

  return st;
}

static SerdStatus
read_LANGTAG(SerdReader* const reader)
{
  int c = peek_byte(reader);
  if (!is_alpha(c)) {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected '%c'", c);
  }

  SerdNode* node = push_node(reader, SERD_LITERAL, "", 0);
  if (!node) {
    return SERD_ERR_OVERFLOW;
  }

  SerdStatus st = SERD_SUCCESS;
  TRY(st, push_byte(reader, node, eat_byte_safe(reader, c)));
  while ((c = peek_byte(reader)) && is_alpha(c)) {
    TRY(st, push_byte(reader, node, eat_byte_safe(reader, c)));
  }
  while (peek_byte(reader) == '-') {
    TRY(st, push_byte(reader, node, eat_byte_safe(reader, '-')));
    while ((c = peek_byte(reader)) && (is_alpha(c) || is_digit(c))) {
      TRY(st, push_byte(reader, node, eat_byte_safe(reader, c)));
    }
  }
  return SERD_SUCCESS;
}

static SerdStatus
read_IRIREF_scheme(SerdReader* const reader, SerdNode* const dest)
{
  int c = peek_byte(reader);
  if (!is_alpha(c)) {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "bad IRI scheme start '%c'", c);
  }

  SerdStatus st = SERD_SUCCESS;
  while ((c = peek_byte(reader)) != EOF) {
    if (c == '>') {
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "missing IRI scheme");
    }

    if (!is_uri_scheme_char(c)) {
      return r_err(reader,
                   SERD_ERR_BAD_SYNTAX,
                   "bad IRI scheme char U+%04X (%c)",
                   (unsigned)c,
                   (char)c);
    }

    if ((st = push_byte(reader, dest, eat_byte_safe(reader, c)))) {
      return st;
    }

    if (c == ':') {
      return SERD_SUCCESS; // End of scheme
    }
  }

  return SERD_FAILURE;
}

static SerdStatus
read_IRIREF(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st = SERD_SUCCESS;
  if ((st = eat_byte_check(reader, '<'))) {
    return st;
  }

  if (!(*dest = push_node(reader, SERD_URI, "", 0))) {
    return SERD_ERR_OVERFLOW;
  }

  if (!fancy_syntax(reader) && (st = read_IRIREF_scheme(reader, *dest))) {
    return r_err(reader, st, "expected IRI scheme");
  }

  uint32_t code = 0;
  while (st <= SERD_FAILURE) {
    const int c = eat_byte_safe(reader, peek_byte(reader));
    switch (c) {
    case '"':
    case '<':
      return r_err(
        reader, SERD_ERR_BAD_SYNTAX, "invalid IRI character '%c'", c);
    case '>':
      return SERD_SUCCESS;
    case '\\':
      if (read_UCHAR(reader, *dest, &code)) {
        return r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid IRI escape");
      }
      switch (code) {
      case 0:
      case ' ':
      case '<':
      case '>':
        return r_err(reader,
                     SERD_ERR_BAD_SYNTAX,
                     "invalid escaped IRI character U+%04X",
                     code);
      default:
        break;
      }
      break;
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      return r_err(
        reader, SERD_ERR_BAD_SYNTAX, "invalid IRI character '%c'", c);
    default:
      if (c <= 0x20) {
        st = r_err(reader,
                   SERD_ERR_BAD_SYNTAX,
                   "invalid IRI character (escape %%%02X)",
                   (unsigned)c);
        if (reader->strict) {
          break;
        }

        if (!(st = push_byte(reader, *dest, c))) {
          st = SERD_FAILURE;
        }
      } else if (!(c & 0x80)) {
        st = push_byte(reader, *dest, c);
      } else {
        st = read_utf8_character(reader, *dest, (uint8_t)c);
      }
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

static SerdStatus
read_PrefixedName(SerdReader* const reader,
                  SerdNode* const   dest,
                  const bool        read_prefix,
                  bool* const       ate_dot)
{
  SerdStatus st = SERD_SUCCESS;
  if (read_prefix && ((st = read_PN_PREFIX(reader, dest)) > SERD_FAILURE)) {
    return st;
  }

  if (peek_byte(reader) != ':') {
    return SERD_FAILURE;
  }

  if ((st = push_byte(reader, dest, eat_byte_safe(reader, ':'))) ||
      (st = read_PN_LOCAL(reader, dest, ate_dot)) > SERD_FAILURE) {
    return st;
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_0_9(SerdReader* const reader, SerdNode* const str, const bool at_least_one)
{
  unsigned   count = 0;
  SerdStatus st    = SERD_SUCCESS;
  for (int c = 0; is_digit((c = peek_byte(reader))); ++count) {
    TRY(st, push_byte(reader, str, eat_byte_safe(reader, c)));
  }

  if (at_least_one && count == 0) {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected digit");
  }

  return st;
}

static SerdStatus
read_number(SerdReader* const reader,
            SerdNode** const  dest,
            bool* const       ate_dot)
{
#define XSD_DECIMAL NS_XSD "decimal"
#define XSD_DOUBLE NS_XSD "double"
#define XSD_INTEGER NS_XSD "integer"

  *dest = push_node(reader, SERD_LITERAL, "", 0);

  SerdStatus st          = SERD_SUCCESS;
  int        c           = peek_byte(reader);
  bool       has_decimal = false;
  if (!*dest) {
    return SERD_ERR_OVERFLOW;
  }

  if (c == '-' || c == '+') {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
  }

  if ((c = peek_byte(reader)) == '.') {
    has_decimal = true;
    // decimal case 2 (e.g. ".0" or "-.0" or "+.0")
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    TRY(st, read_0_9(reader, *dest, true));
  } else {
    // all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
    TRY(st, read_0_9(reader, *dest, true));
    if ((c = peek_byte(reader)) == '.') {
      has_decimal = true;

      // Annoyingly, dot can be end of statement, so tentatively eat
      skip_byte(reader, c);
      c = peek_byte(reader);
      if (!is_digit(c) && c != 'e' && c != 'E') {
        *ate_dot = true;     // Force caller to deal with stupid grammar
        return SERD_SUCCESS; // Next byte is not a number character
      }

      TRY(st, push_byte(reader, *dest, '.'));
      read_0_9(reader, *dest, false);
    }
  }
  c = peek_byte(reader);
  if (c == 'e' || c == 'E') {
    // double
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    switch ((c = peek_byte(reader))) {
    case '+':
    case '-':
      TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
      break;
    default:
      break;
    }
    TRY(st, read_0_9(reader, *dest, true));
    push_node(reader, SERD_URI, XSD_DOUBLE, sizeof(XSD_DOUBLE) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  } else if (has_decimal) {
    push_node(reader, SERD_URI, XSD_DECIMAL, sizeof(XSD_DECIMAL) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  } else {
    push_node(reader, SERD_URI, XSD_INTEGER, sizeof(XSD_INTEGER) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_iri(SerdReader* const reader, SerdNode** const dest, bool* const ate_dot)
{
  switch (peek_byte(reader)) {
  case '<':
    return read_IRIREF(reader, dest);
  default:
    *dest = push_node(reader, SERD_CURIE, "", 0);
    return *dest ? read_PrefixedName(reader, *dest, true, ate_dot)
                 : SERD_ERR_OVERFLOW;
  }
}

static SerdStatus
read_literal(SerdReader* const reader,
             SerdNode** const  dest,
             bool* const       ate_dot)
{
  if (!(*dest = push_node(reader, SERD_LITERAL, "", 0))) {
    return SERD_ERR_OVERFLOW;
  }

  SerdStatus st = read_String(reader, *dest);
  if (st) {
    return st;
  }

  SerdNode* datatype = NULL;
  switch (peek_byte(reader)) {
  case '@':
    skip_byte(reader, '@');
    (*dest)->flags |= SERD_HAS_LANGUAGE;
    TRY(st, read_LANGTAG(reader));
    break;
  case '^':
    skip_byte(reader, '^');
    TRY(st, eat_byte_check(reader, '^'));
    (*dest)->flags |= SERD_HAS_DATATYPE;
    TRY(st, read_iri(reader, &datatype, ate_dot));
    break;
  }
  return SERD_SUCCESS;
}

static SerdStatus
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

static SerdStatus
read_Var(SerdReader* const reader, SerdNode** const dest)
{
  if (!(reader->flags & SERD_READ_VARIABLES)) {
    return r_err(
      reader, SERD_ERR_BAD_SYNTAX, "syntax does not support variables");
  }

  if (!(*dest = push_node(reader, SERD_VARIABLE, "", 0))) {
    return SERD_ERR_OVERFLOW;
  }

  assert(peek_byte(reader) == '$' || peek_byte(reader) == '?');
  serd_byte_source_advance(reader->source);

  return read_VARNAME(reader, dest);
}

static SerdStatus
read_verb(SerdReader* reader, SerdNode** dest)
{
  const size_t orig_stack_size = reader->stack.size;

  switch (peek_byte(reader)) {
  case '$':
  case '?':
    return read_Var(reader, dest);
  case '<':
    return read_IRIREF(reader, dest);
  }

  /* Either a qname, or "a".  Read the prefix first, and if it is in fact
     "a", produce that instead.
  */
  if (!(*dest = push_node(reader, SERD_CURIE, "", 0))) {
    return SERD_ERR_OVERFLOW;
  }

  SerdStatus st = read_PN_PREFIX(reader, *dest);
  if (st > SERD_FAILURE) {
    return st;
  }

  bool      ate_dot = false;
  SerdNode* node    = *dest;
  const int next    = peek_byte(reader);
  if (node->length == 1 && serd_node_string(node)[0] == 'a' && next != ':' &&
      !is_PN_CHARS_BASE((uint32_t)next)) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);
    return ((*dest = push_node(reader, SERD_URI, NS_RDF "type", 47))
              ? SERD_SUCCESS
              : SERD_ERR_OVERFLOW);
  }

  if ((st = read_PrefixedName(reader, *dest, false, &ate_dot)) || ate_dot) {
    *dest = NULL;
    return r_err(
      reader, st > SERD_FAILURE ? st : SERD_ERR_BAD_SYNTAX, "expected verb");
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_BLANK_NODE_LABEL(SerdReader* const reader,
                      SerdNode** const  dest,
                      bool* const       ate_dot)
{
  SerdStatus st = SERD_SUCCESS;

  skip_byte(reader, '_');
  TRY(st, eat_byte_check(reader, ':'));

  if (!(*dest = push_node(reader,
                          SERD_BLANK,
                          reader->bprefix ? reader->bprefix : "",
                          reader->bprefix_len))) {
    return SERD_ERR_OVERFLOW;
  }

  SerdNode* n = *dest;
  int       c = peek_byte(reader); // First: (PN_CHARS | '_' | [0-9])
  if (is_digit(c) || c == '_') {
    TRY(st, push_byte(reader, n, eat_byte_safe(reader, c)));
  } else if ((st = read_PN_CHARS(reader, n))) {
    return r_err(reader, st, "invalid name start");
  }

  while ((c = peek_byte(reader))) { // Middle: (PN_CHARS | '.')*
    if (c == '.') {
      TRY(st, push_byte(reader, n, eat_byte_safe(reader, c)));
    } else if ((st = read_PN_CHARS(reader, n))) {
      break;
    }
  }

  if (st > SERD_FAILURE) {
    return st;
  }

  char* buf = serd_node_buffer(n);
  if (buf[n->length - 1] == '.' && read_PN_CHARS(reader, n)) {
    // Ate trailing dot, pop it from stack/node and inform caller
    --n->length;
    serd_stack_pop(&reader->stack, 1);
    *ate_dot = true;
  }

  if (fancy_syntax(reader) && !(reader->flags & SERD_READ_EXACT_BLANKS)) {
    if (is_digit(buf[reader->bprefix_len + 1])) {
      if ((buf[reader->bprefix_len]) == 'b') {
        buf[reader->bprefix_len] = 'B'; // Prevent clash
        reader->seen_genid       = true;
      } else if (reader->seen_genid && buf[reader->bprefix_len] == 'B') {
        return r_err(reader,
                     SERD_ERR_ID_CLASH,
                     "found both 'b' and 'B' blank IDs, prefix required");
      }
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

static SerdStatus
read_anon(SerdReader* const reader,
          ReadContext       ctx,
          const bool        subject,
          SerdNode** const  dest)
{
  skip_byte(reader, '[');

  const SerdStatementFlags old_flags = *ctx.flags;
  const bool               empty     = peek_delim(reader, ']');

  if (subject) {
    *ctx.flags |= empty ? SERD_EMPTY_S : SERD_ANON_S;
  } else {
    *ctx.flags |= SERD_ANON_O;
  }

  if (!*dest) {
    if (!(*dest = blank_id(reader))) {
      return SERD_ERR_OVERFLOW;
    }
  }

  // Emit statement with this anonymous object first
  SerdStatus st = SERD_SUCCESS;
  if (ctx.subject) {
    TRY(st, emit_statement(reader, ctx, *dest));
  }

  // Switch the subject to the anonymous node and read its description
  ctx.subject = *dest;
  if (!empty) {
    bool ate_dot_in_list = false;
    TRY(st, read_predicateObjectList(reader, ctx, &ate_dot_in_list));

    if (ate_dot_in_list) {
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "'.' inside blank");
    }

    read_ws_star(reader);
    *ctx.flags = old_flags;
  }

  if (!(subject && empty)) {
    TRY(st, serd_sink_write_end(reader->sink, *dest));
  }

  return eat_byte_check(reader, ']');
}

/* If emit is true: recurses, calling statement_sink for every statement
   encountered, and leaves stack in original calling state (i.e. pops
   everything it pushes). */
static SerdStatus
read_object(SerdReader* const  reader,
            ReadContext* const ctx,
            const bool         emit,
            bool* const        ate_dot)
{
  static const char* const XSD_BOOLEAN     = NS_XSD "boolean";
  static const size_t      XSD_BOOLEAN_LEN = 40;

  const size_t orig_stack_size = reader->stack.size;

  SerdStatus ret    = SERD_FAILURE;
  bool       simple = (ctx->subject != 0);
  SerdNode*  o      = 0;
  const int  c      = peek_byte(reader);
  if (!fancy_syntax(reader)) {
    switch (c) {
    case '"':
    case '$':
    case ':':
    case '<':
    case '?':
    case '_':
      break;
    default:
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected: ':', '<', or '_'");
    }
  }

  switch (c) {
  case EOF:
  case ')':
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected object");
  case '$':
  case '?':
    ret = read_Var(reader, &o);
    break;
  case '[':
    simple = false;
    ret    = read_anon(reader, *ctx, false, &o);
    break;
  case '(':
    simple = false;
    ret    = read_collection(reader, *ctx, &o);
    break;
  case '_':
    ret = read_BLANK_NODE_LABEL(reader, &o, ate_dot);
    break;
  case '<':
    ret = read_IRIREF(reader, &o);
    break;
  case ':':
    ret = read_iri(reader, &o, ate_dot);
    break;
  case '+':
  case '-':
  case '.':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    ret = read_number(reader, &o, ate_dot);
    break;
  case '\"':
  case '\'':
    ret = read_literal(reader, &o, ate_dot);
    break;
  default:
    /* Either a boolean literal, or a qname.  Read the prefix first, and if
       it is in fact a "true" or "false" literal, produce that instead.
    */
    if (!(o = push_node(reader, SERD_CURIE, "", 0))) {
      return SERD_ERR_OVERFLOW;
    }

    while (!(ret = read_PN_CHARS_BASE(reader, o))) {
    }

    if (ret > SERD_FAILURE) {
      return ret;
    }

    if ((o->length == 4 && !memcmp(serd_node_string(o), "true", 4)) ||
        (o->length == 5 && !memcmp(serd_node_string(o), "false", 5))) {
      o->flags |= SERD_HAS_DATATYPE;
      o->type = SERD_LITERAL;
      if (!(push_node(reader, SERD_URI, XSD_BOOLEAN, XSD_BOOLEAN_LEN))) {
        ret = SERD_ERR_OVERFLOW;
      } else {
        ret = SERD_SUCCESS;
      }
    } else if ((ret = read_PN_PREFIX_tail(reader, o)) > SERD_FAILURE ||
               (ret = read_PrefixedName(reader, o, false, ate_dot))) {
      ret = (ret > SERD_FAILURE) ? ret : SERD_ERR_BAD_SYNTAX;
      return r_err(reader, ret, "expected prefixed name");
    }
  }

  if (!ret && emit && simple && o) {
    ret = emit_statement(reader, *ctx, o);
  } else if (!ret && !emit) {
    ctx->object = o;
    return SERD_SUCCESS;
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);
#ifndef NDEBUG
  assert(reader->stack.size == orig_stack_size);
#endif
  return ret;
}

static SerdStatus
read_objectList(SerdReader* const reader, ReadContext ctx, bool* const ate_dot)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, read_object(reader, &ctx, true, ate_dot));
  if (!fancy_syntax(reader) && peek_delim(reader, ',')) {
    return r_err(
      reader, SERD_ERR_BAD_SYNTAX, "syntax does not support abbreviation");
  }

  while (st <= SERD_FAILURE && !*ate_dot && eat_delim(reader, ',')) {
    st = read_object(reader, &ctx, true, ate_dot);
  }

  return st;
}

static SerdStatus
read_predicateObjectList(SerdReader* const reader,
                         ReadContext       ctx,
                         bool* const       ate_dot)
{
  const size_t orig_stack_size = reader->stack.size;

  SerdStatus st = SERD_SUCCESS;
  while (!(st = read_verb(reader, &ctx.predicate)) && read_ws_star(reader) &&
         !(st = read_objectList(reader, ctx, ate_dot))) {
    if (*ate_dot) {
      serd_stack_pop_to(&reader->stack, orig_stack_size);
      return SERD_SUCCESS;
    }

    bool ate_semi = false;
    int  c        = 0;
    do {
      read_ws_star(reader);
      switch (c = peek_byte(reader)) {
      case EOF:
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected end of file");
      case '.':
      case ']':
      case '}':
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return SERD_SUCCESS;
      case ';':
        skip_byte(reader, c);
        ate_semi = true;
      }
    } while (c == ';');

    if (!ate_semi) {
      serd_stack_pop_to(&reader->stack, orig_stack_size);
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "missing ';' or '.'");
    }
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);
  ctx.predicate = 0;
  return st;
}

static SerdStatus
end_collection(SerdReader* const reader, const SerdStatus st)
{
  return st ? st : eat_byte_check(reader, ')');
}

static SerdStatus
read_collection(SerdReader* const reader,
                ReadContext       ctx,
                SerdNode** const  dest)
{
  SerdStatus st = SERD_SUCCESS;

  skip_byte(reader, '(');

  bool end = peek_delim(reader, ')');
  *dest    = end ? reader->rdf_nil : blank_id(reader);

  if (!*dest) {
    return SERD_ERR_OVERFLOW;
  }

  if (ctx.subject) {
    // subject predicate _:head
    *ctx.flags |= (end ? 0 : SERD_LIST_O);
    TRY(st, emit_statement(reader, ctx, *dest));
  } else {
    *ctx.flags |= (end ? 0 : SERD_LIST_S);
  }

  if (end) {
    return end_collection(reader, st);
  }

  /* The order of node allocation here is necessarily not in stack order,
     so we create two nodes and recycle them throughout. */
  SerdNode* n1 =
    push_node_padded(reader, genid_length(reader), SERD_BLANK, "", 0);

  SerdNode* node = n1;
  SerdNode* rest = 0;

  if (!n1) {
    return SERD_ERR_OVERFLOW;
  }

  ctx.subject = *dest;
  while (!peek_delim(reader, ')')) {
    // _:node rdf:first object
    ctx.predicate = reader->rdf_first;
    bool ate_dot  = false;
    if ((st = read_object(reader, &ctx, true, &ate_dot)) || ate_dot) {
      return end_collection(reader, st);
    }

    if (!(end = peek_delim(reader, ')'))) {
      /* Give rest a new ID.  Done as late as possible to ensure it is
         used and > IDs generated by read_object above. */
      if (!rest) {
        rest = blank_id(reader); // First pass, push
        assert(rest);            // Can't overflow since read_object() popped
      } else {
        set_blank_id(reader, rest, genid_length(reader) + 1);
      }
    }

    // _:node rdf:rest _:rest
    ctx.predicate = reader->rdf_rest;
    TRY(st, emit_statement(reader, ctx, (end ? reader->rdf_nil : rest)));

    ctx.subject = rest;        // _:node = _:rest
    rest        = node;        // _:rest = (old)_:node
    node        = ctx.subject; // invariant
  }

  return end_collection(reader, st);
}

static SerdStatus
read_subject(SerdReader* const reader,
             ReadContext       ctx,
             SerdNode** const  dest,
             int* const        s_type)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;
  switch ((*s_type = peek_byte(reader))) {
  case '$':
  case '?':
    st = read_Var(reader, dest);
    break;
  case '[':
    st = read_anon(reader, ctx, true, dest);
    break;
  case '(':
    st = read_collection(reader, ctx, dest);
    break;
  case '_':
    st = read_BLANK_NODE_LABEL(reader, dest, &ate_dot);
    break;
  default:
    st = read_iri(reader, dest, &ate_dot);
  }

  if (ate_dot) {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "subject ends with '.'");
  }

  return st;
}

static SerdStatus
read_labelOrSubject(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;

  switch (peek_byte(reader)) {
  case '[':
    skip_byte(reader, '[');
    read_ws_star(reader);
    if ((st = eat_byte_check(reader, ']'))) {
      return st;
    }
    *dest = blank_id(reader);
    return *dest ? SERD_SUCCESS : SERD_ERR_OVERFLOW;
  case '_':
    return read_BLANK_NODE_LABEL(reader, dest, &ate_dot);
  default:
    if (!read_iri(reader, dest, &ate_dot)) {
      return SERD_SUCCESS;
    } else {
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected label or subject");
    }
  }
}

static SerdStatus
read_triples(SerdReader* const reader, ReadContext ctx, bool* const ate_dot)
{
  SerdStatus st = SERD_FAILURE;
  if (ctx.subject) {
    read_ws_star(reader);
    switch (peek_byte(reader)) {
    case '.':
      *ate_dot = eat_byte_safe(reader, '.');
      return SERD_FAILURE;
    case '}':
      return SERD_FAILURE;
    }
    st = read_predicateObjectList(reader, ctx, ate_dot);
  }

  ctx.subject = ctx.predicate = 0;
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
read_base(SerdReader* const reader, const bool sparql, const bool token)
{
  SerdStatus st = SERD_SUCCESS;
  if (token) {
    TRY(st, eat_string(reader, "base", 4));
  }

  read_ws_star(reader);

  SerdNode* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));

  if (reader->stack.size + sizeof(SerdNode) > reader->stack.buf_size) {
    return SERD_ERR_OVERFLOW;
  }

  serd_node_zero_pad(uri);
  TRY(st, serd_env_set_base_uri(reader->env, serd_node_string_view(uri)));
  TRY(st, serd_sink_write_base(reader->sink, uri));

  read_ws_star(reader);
  if (!sparql) {
    return eat_byte_check(reader, '.');
  }

  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "full stop after SPARQL BASE");
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_prefixID(SerdReader* const reader, const bool sparql, const bool token)
{
  SerdStatus st = SERD_SUCCESS;
  if (token) {
    TRY(st, eat_string(reader, "prefix", 6));
  }

  read_ws_star(reader);
  SerdNode* name = push_node(reader, SERD_LITERAL, "", 0);
  if (!name) {
    return SERD_ERR_OVERFLOW;
  }

  if ((st = read_PN_PREFIX(reader, name)) > SERD_FAILURE) {
    return st;
  }

  if ((st = eat_byte_check(reader, ':'))) {
    return st;
  }

  read_ws_star(reader);
  SerdNode* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));

  if (reader->stack.size + sizeof(SerdNode) > reader->stack.buf_size) {
    return SERD_ERR_OVERFLOW;
  }

  serd_node_zero_pad(name);
  serd_node_zero_pad(uri);

  TRY(st,
      serd_env_set_prefix(
        reader->env, serd_node_string_view(name), serd_node_string_view(uri)));

  st = serd_sink_write_prefix(reader->sink, name, uri);

  if (!sparql) {
    read_ws_star(reader);
    st = eat_byte_check(reader, '.');
  }
  return st;
}

static SerdStatus
read_directive(SerdReader* const reader)
{
  const bool sparql = peek_byte(reader) != '@';
  if (!sparql) {
    skip_byte(reader, '@');
    switch (peek_byte(reader)) {
    case 'B':
    case 'P':
      return r_err(reader, SERD_ERR_BAD_SYNTAX, "uppercase directive");
    }
  }

  switch (peek_byte(reader)) {
  case 'B':
  case 'b':
    return read_base(reader, sparql, true);
  case 'P':
  case 'p':
    return read_prefixID(reader, sparql, true);
  default:
    break;
  }

  return r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid directive");
}

static SerdStatus
read_wrappedGraph(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;
  if ((st = eat_byte_check(reader, '{'))) {
    return st;
  }

  read_ws_star(reader);
  while (peek_byte(reader) != '}') {
    const size_t orig_stack_size = reader->stack.size;
    bool         ate_dot         = false;
    int          s_type          = 0;

    ctx->subject = 0;
    if ((st = read_subject(reader, *ctx, &ctx->subject, &s_type))) {
      return r_err(reader, st, "expected subject");
    }

    if (read_triples(reader, *ctx, &ate_dot) && s_type != '[') {
      return r_err(
        reader, SERD_ERR_BAD_SYNTAX, "missing predicate object list");
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
    read_ws_star(reader);
    if (peek_byte(reader) == '.') {
      skip_byte(reader, '.');
    }
    read_ws_star(reader);
  }

  skip_byte(reader, '}');
  read_ws_star(reader);
  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_ERR_BAD_SYNTAX, "graph followed by '.'");
  }

  return SERD_SUCCESS;
}

static int
tokcmp(SerdNode* const node, const char* const tok, const size_t n)
{
  return ((!node || node->length != n)
            ? -1
            : serd_strncasecmp(serd_node_string(node), tok, n));
}

SerdStatus
read_n3_statement(SerdReader* const reader)
{
  SerdStatementFlags flags   = 0;
  ReadContext        ctx     = {0, 0, 0, 0, &flags};
  bool               ate_dot = false;
  int                s_type  = 0;
  SerdStatus         st      = SERD_SUCCESS;
  read_ws_star(reader);
  switch (peek_byte(reader)) {
  case '\0':
    skip_byte(reader, '\0');
    return SERD_FAILURE;
  case EOF:
    return SERD_FAILURE;
  case '@':
    if (!fancy_syntax(reader)) {
      return r_err(
        reader, SERD_ERR_BAD_SYNTAX, "syntax does not support directives");
    }
    TRY(st, read_directive(reader));
    read_ws_star(reader);
    break;
  case '{':
    if (reader->syntax == SERD_TRIG) {
      TRY(st, read_wrappedGraph(reader, &ctx));
      read_ws_star(reader);
    } else {
      return r_err(
        reader, SERD_ERR_BAD_SYNTAX, "syntax does not support graphs");
    }
    break;
  default:
    if ((st = read_subject(reader, ctx, &ctx.subject, &s_type)) >
        SERD_FAILURE) {
      return st;
    }

    if (!tokcmp(ctx.subject, "base", 4)) {
      st = read_base(reader, true, false);
    } else if (!tokcmp(ctx.subject, "prefix", 6)) {
      st = read_prefixID(reader, true, false);
    } else if (!tokcmp(ctx.subject, "graph", 5)) {
      ctx.subject = NULL;
      read_ws_star(reader);
      TRY(st, read_labelOrSubject(reader, &ctx.graph));
      read_ws_star(reader);
      TRY(st, read_wrappedGraph(reader, &ctx));
      ctx.graph = 0;
      read_ws_star(reader);
    } else if (read_ws_star(reader) && peek_byte(reader) == '{') {
      if (s_type == '(' || (s_type == '[' && !*ctx.flags)) {
        return r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid graph name");
      }

      ctx.graph   = ctx.subject;
      ctx.subject = NULL;
      TRY(st, read_wrappedGraph(reader, &ctx));
      read_ws_star(reader);
    } else if ((st = read_triples(reader, ctx, &ate_dot))) {
      if (st == SERD_FAILURE && s_type == '[') {
        return SERD_SUCCESS;
      }

      if (ate_dot && (reader->strict || (s_type != '('))) {
        return r_err(
          reader, SERD_ERR_BAD_SYNTAX, "unexpected end of statement");
      }

      return st > SERD_FAILURE ? st : SERD_ERR_BAD_SYNTAX;

    } else if (!ate_dot) {
      read_ws_star(reader);
      st = eat_byte_check(reader, '.');
    }
    break;
  }

  return st;
}

SerdStatus
serd_reader_skip_until_byte(SerdReader* const reader, const uint8_t byte)
{
  int c = peek_byte(reader);

  while (c != byte && c != EOF) {
    skip_byte(reader, c);
    c = peek_byte(reader);
  }

  return c == EOF ? SERD_FAILURE : SERD_SUCCESS;
}

SerdStatus
read_turtleTrigDoc(SerdReader* const reader)
{
  while (!reader->source->eof) {
    const size_t     orig_stack_size = reader->stack.size;
    const SerdStatus st              = read_n3_statement(reader);

    if (st > SERD_FAILURE) {
      if (!tolerate_status(reader, st)) {
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return st;
      }
      serd_reader_skip_until_byte(reader, '\n');
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
  }

  return SERD_SUCCESS;
}

SerdStatus
read_nquadsDoc(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;
  while (!st && !reader->source->eof) {
    const size_t orig_stack_size = reader->stack.size;

    SerdStatementFlags flags   = 0;
    ReadContext        ctx     = {0, 0, 0, 0, &flags};
    bool               ate_dot = false;
    int                s_type  = 0;
    read_ws_star(reader);
    if (peek_byte(reader) == EOF) {
      break;
    }

    if (peek_byte(reader) == '@') {
      r_err(reader, SERD_ERR_BAD_SYNTAX, "syntax does not support directives");
      return SERD_ERR_BAD_SYNTAX;
    }

    if ((st = read_subject(reader, ctx, &ctx.subject, &s_type)) ||
        !read_ws_star(reader)) {
      return st;
    }

    switch (peek_byte(reader)) {
    case '$':
    case '?':
      st = read_Var(reader, &ctx.predicate);
      break;
    case '<':
      st = read_IRIREF(reader, &ctx.predicate);
      break;
    }

    if (st || !read_ws_star(reader) ||
        (st = read_object(reader, &ctx, false, &ate_dot))) {
      return st;
    }

    if (!ate_dot) { // graphLabel?
      read_ws_star(reader);
      switch (peek_byte(reader)) {
      case '.':
        break;
      case '?':
        TRY(st, read_Var(reader, &ctx.graph));
        break;
      case '_':
        TRY(st, read_BLANK_NODE_LABEL(reader, &ctx.graph, &ate_dot));
        break;
      default:
        TRY(st, read_IRIREF(reader, &ctx.graph));
      }

      // Terminating '.'
      read_ws_star(reader);
      TRY(st, eat_byte_check(reader, '.'));
    }

    st = emit_statement(reader, ctx, ctx.object);
    serd_stack_pop_to(&reader->stack, orig_stack_size);
  }
  return st;
}

#if defined(__clang__) && __clang_major__ >= 10
_Pragma("clang diagnostic pop")
#endif
