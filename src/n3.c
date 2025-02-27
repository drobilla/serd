// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"
#include "stack.h"
#include "string_utils.h"
#include "symbols.h"
#include "token_header.h"
#include "try.h"

#include <serd/event.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/reader.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

static const uint8_t replacement_char[] = {0xEFU, 0xBFU, 0xBDU};

static bool
fancy_syntax(const SerdReader* const reader)
{
  return reader->syntax == SERD_TURTLE || reader->syntax == SERD_TRIG;
}

static SerdStatus
read_collection(SerdReader* reader, ReadContext ctx, TokenHeader** dest);

static SerdStatus
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot);

static uint8_t
read_HEX(SerdReader* const reader)
{
  const int c = peek_byte(reader);
  if (is_xdigit(c)) {
    return (uint8_t)eat_byte_safe(reader, c);
  }

  r_err_char(reader, "hexadecimal", c);
  return 0;
}

// Read UCHAR escape, initial \ is already eaten by caller
static SerdStatus
read_UCHAR(SerdReader* const  reader,
           TokenHeader* const dest,
           uint32_t* const    char_code)
{
  const int b      = peek_byte(reader);
  unsigned  length = 0;
  if (b == 'U') {
    length = 8;
  } else if (b == 'u') {
    length = 4;
  } else {
    return SERD_BAD_SYNTAX;
  }

  skip_byte(reader, b);

  // Read character code point in hex
  uint8_t  buf[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t code   = 0U;
  for (unsigned i = 0U; i < length; ++i) {
    if (!(buf[i] = read_HEX(reader))) {
      return SERD_BAD_SYNTAX;
    }

    code = (code << (i ? 4U : 0U)) | hex_digit_value(buf[i]);
  }

  // Determine the encoded size from the code point
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
    r_err(reader, SERD_BAD_SYNTAX, "unicode character 0x%X out of range", code);
    *char_code = 0xFFFD;
    return reader->strict ? SERD_BAD_SYNTAX
                          : push_bytes(reader, dest, replacement_char, 3);
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
read_ECHAR(SerdReader* const reader, TokenHeader* const dest)
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

static SerdStatus
bad_char(SerdReader* const reader, const char* const fmt, const uint8_t c)
{
  // Skip bytes until the next start byte
  for (int b = peek_byte(reader); b != EOF && ((uint8_t)b & 0x80);) {
    skip_byte(reader, b);
    b = peek_byte(reader);
  }

  r_err(reader, SERD_BAD_SYNTAX, fmt, c);
  return reader->strict ? SERD_BAD_SYNTAX : SERD_FAILURE;
}

static SerdStatus
read_utf8_bytes(SerdReader* const reader,
                uint8_t           bytes[4],
                uint8_t* const    size,
                const uint8_t     c)
{
  *size = utf8_num_bytes(c);
  if (*size <= 1) {
    return bad_char(reader, "invalid UTF-8 start 0x%X", c);
  }

  bytes[0] = c;
  for (uint8_t i = 1U; i < *size; ++i) {
    const int b = peek_byte(reader);
    if (b == EOF || ((uint8_t)b & 0x80U) == 0U) {
      return bad_char(reader, "invalid UTF-8 continuation 0x%X", (uint8_t)b);
    }

    bytes[i] = (uint8_t)eat_byte_safe(reader, b);
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_utf8_character(SerdReader* const  reader,
                    TokenHeader* const dest,
                    const uint8_t      c)
{
  uint8_t    size     = 0U;
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
read_utf8_code(SerdReader* const  reader,
               TokenHeader* const dest,
               uint32_t* const    code,
               const uint8_t      c)
{
  uint8_t    size     = 0U;
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
read_character(SerdReader* const  reader,
               TokenHeader* const dest,
               const uint8_t      c)
{
  if (!(c & 0x80)) {
    if (c == 0xA || c == 0xD) {
      dest->flags |= SERD_HAS_NEWLINE;
    } else if (c == '"' || c == '\'') {
      dest->flags |= SERD_HAS_QUOTE;
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
  while (((c = peek_byte(reader)) > 0) && c != 0xA && c != 0xD) {
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

static SerdStatus
read_string_escape(SerdReader* const reader, TokenHeader* const ref)
{
  SerdStatus st   = SERD_SUCCESS;
  uint32_t   code = 0;
  if ((st = read_ECHAR(reader, ref)) == SERD_BAD_SYNTAX &&
      (st = read_UCHAR(reader, ref, &code))) {
    return r_err(reader, st, "expected string escape sequence");
  }

  return st;
}

// STRING_LITERAL_LONG_QUOTE and STRING_LITERAL_LONG_SINGLE_QUOTE
// Initial triple quotes are already eaten by caller
static SerdStatus
read_STRING_LITERAL_LONG(SerdReader* const  reader,
                         TokenHeader* const ref,
                         const uint8_t      q)
{
  SerdStatus st = SERD_SUCCESS;
  while (tolerate_status(reader, st)) {
    const int c = peek_byte(reader);
    if (c == '\\') {
      if (!(st = skip_byte(reader, c))) {
        st = read_string_escape(reader, ref);
      }
    } else if (c == q) {
      skip_byte(reader, q);
      const int q2 = eat_byte_safe(reader, peek_byte(reader));
      const int q3 = peek_byte(reader);
      if (q2 == q && q3 == q) { // End of string
        skip_byte(reader, q3);
        break;
      }

      if (q2 == '\\') {
        if (!(st = push_byte(reader, ref, c))) {
          st = read_string_escape(reader, ref);
        }
      } else {
        ref->flags |= SERD_HAS_QUOTE;
        if (!(st = push_byte(reader, ref, c))) {
          st = read_character(reader, ref, (uint8_t)q2);
        }
      }
    } else if (c > 0) {
      st = read_character(reader, ref, (uint8_t)eat_byte_safe(reader, c));
    } else {
      return r_err(reader, SERD_BAD_SYNTAX, "end of file in long string");
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

// STRING_LITERAL_QUOTE and STRING_LITERAL_SINGLE_QUOTE
// Initial quote is already eaten by caller
static SerdStatus
read_STRING_LITERAL(SerdReader* const  reader,
                    TokenHeader* const ref,
                    const uint8_t      q)
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

static SerdStatus
read_String(SerdReader* const reader, TokenHeader* const node)
{
  const int q1 = eat_byte_safe(reader, peek_byte(reader));
  const int q2 = peek_byte(reader);
  if (q2 == EOF) {
    return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
  }

  if (q2 != q1) { // Short string (not triple quoted)
    return read_STRING_LITERAL(reader, node, (uint8_t)q1);
  }

  skip_byte(reader, q2);
  const int q3 = peek_byte(reader);
  if (q3 == EOF) {
    return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
  }

  if (q3 != q1) { // Empty short string ("" or '')
    return SERD_SUCCESS;
  }

  if (!fancy_syntax(reader)) {
    return r_err(
      reader, SERD_BAD_SYNTAX, "syntax does not support long literals");
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
read_PN_CHARS_BASE(SerdReader* const reader, TokenHeader* const dest)
{
  uint32_t   code = 0;
  const int  c    = peek_byte(reader);
  SerdStatus st   = SERD_SUCCESS;

  if (is_alpha(c)) {
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  }

  if (c < 0x80) {
    return SERD_FAILURE;
  }

  skip_byte(reader, c);
  TRY(st, read_utf8_code(reader, dest, &code, (uint8_t)c));

  if (!is_PN_CHARS_BASE(code)) {
    st = r_err_char(reader, "name", (int)code);
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
read_PN_CHARS(SerdReader* const reader, TokenHeader* const dest)
{
  uint32_t   code = 0;
  const int  c    = peek_byte(reader);
  SerdStatus st   = SERD_SUCCESS;

  if (is_alpha(c) || is_digit(c) || c == '_' || c == '-') {
    return push_byte(reader, dest, eat_byte_safe(reader, c));
  }

  if (c < 0x80) {
    return SERD_FAILURE;
  }

  skip_byte(reader, c);
  TRY(st, read_utf8_code(reader, dest, &code, (uint8_t)c));

  if (!is_PN_CHARS(code)) {
    st = r_err_char(reader, "name", (int)code);
  }

  return st;
}

static SerdStatus
read_PERCENT(SerdReader* const reader, TokenHeader* const dest)
{
  SerdStatus st = push_byte(reader, dest, eat_byte_safe(reader, '%'));
  if (st) {
    return st;
  }

  const uint8_t h1 = read_HEX(reader);
  const uint8_t h2 = read_HEX(reader);
  if (!h1 || !h2) {
    return SERD_BAD_SYNTAX;
  }

  if (!(st = push_byte(reader, dest, h1))) {
    st = push_byte(reader, dest, h2);
  }

  return st;
}

static SerdStatus
read_PN_LOCAL_ESC(SerdReader* const reader, TokenHeader* const dest)
{
  skip_byte(reader, '\\');

  const int c = peek_byte(reader);
  return ((c == '!') || in_range(c, '#', '/') || (c == ';') || (c == '=') ||
          (c == '?') || (c == '@') || (c == '_') || (c == '~'))
           ? push_byte(reader, dest, eat_byte_safe(reader, c))
           : r_err(reader, SERD_BAD_SYNTAX, "bad escape");
}

static SerdStatus
read_PLX(SerdReader* const reader, TokenHeader* const dest)
{
  const int c = peek_byte(reader);

  return (c == '%')    ? read_PERCENT(reader, dest)
         : (c == '\\') ? read_PN_LOCAL_ESC(reader, dest)
                       : SERD_FAILURE;
}

static SerdStatus
read_PN_LOCAL(SerdReader* const  reader,
              TokenHeader* const dest,
              bool* const        ate_dot)
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
    }

    if (st != SERD_SUCCESS && (st = read_PN_CHARS_BASE(reader, dest))) {
      return st;
    }
  }

  while ((c = peek_byte(reader)) > 0) { // Middle: (PN_CHARS | '.' | ':')*
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
    *ate_dot = pop_last_node_char(reader, dest);
  }

  return (st > SERD_FAILURE) ? st : SERD_SUCCESS;
}

// Read the remainder of a PN_PREFIX after some initial characters
static SerdStatus
read_PN_PREFIX_tail(SerdReader* const  reader,
                    TokenHeader* const dest,
                    bool* const        ate_dot)
{
  SerdStatus st                     = SERD_SUCCESS;
  bool       trailing_unescaped_dot = false;

  while (!st) { // Middle: (PN_CHARS | '.')*
    const int c = peek_byte(reader);
    if (c == '.') {
      st = push_byte(reader, dest, eat_byte_safe(reader, c));
      trailing_unescaped_dot = true;
    } else if (!(st = read_PN_CHARS(reader, dest))) {
      trailing_unescaped_dot = false;
    }
  }

  if (trailing_unescaped_dot) {
    *ate_dot = pop_last_node_char(reader, dest);
  }

  return st;
}

static SerdStatus
read_PN_PREFIX(SerdReader* const  reader,
               TokenHeader* const dest,
               bool* const        ate_dot)
{
  const SerdStatus st = read_PN_CHARS_BASE(reader, dest);

  return st ? st : read_PN_PREFIX_tail(reader, dest, ate_dot);
}

static SerdStatus
read_LANGTAG(SerdReader* const reader, TokenHeader** const dest)
{
  int c = peek_byte(reader);
  if (!is_alpha(c)) {
    return r_err_char(reader, "language", c);
  }

  if (!(*dest = push_node_head(reader, SERD_LITERAL))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st = SERD_SUCCESS;
  TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
  while (((c = peek_byte(reader)) > 0) && is_alpha(c)) {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
  }

  while (peek_byte(reader) == '-') {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, '-')));
    while (((c = peek_byte(reader)) > 0) && (is_alpha(c) || is_digit(c))) {
      TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    }
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_IRIREF_scheme(SerdReader* const reader, TokenHeader* const dest)
{
  int c = peek_byte(reader);
  if (!is_alpha(c)) {
    return r_err_char(reader, "IRI scheme start", c);
  }

  SerdStatus st = SERD_SUCCESS;
  while ((c = peek_byte(reader)) > 0) {
    if (c == '>') {
      return r_err(reader, SERD_BAD_SYNTAX, "missing IRI scheme");
    }

    if (c == ':') { // End of scheme
      return push_byte(reader, dest, eat_byte_safe(reader, ':'));
    }

    if (!is_scheme(c)) {
      return r_err_char(reader, "IRI scheme", c);
    }

    if ((st = push_byte(reader, dest, eat_byte_safe(reader, c)))) {
      return st;
    }
  }

  return SERD_FAILURE;
}

static SerdStatus
read_IRIREF(SerdReader* const reader, TokenHeader** const dest)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, eat_byte_check(reader, '<'));

  if (!(*dest = push_node_head(reader, SERD_URI))) {
    return SERD_BAD_STACK;
  }

  if (!fancy_syntax(reader) &&
      (st = read_IRIREF_scheme(reader, *dest)) > SERD_FAILURE) {
    return r_err(reader, st, "expected IRI scheme");
  }

  uint32_t code = 0;
  while (st <= SERD_FAILURE) {
    const int c = eat_byte_safe(reader, peek_byte(reader));
    switch (c) {
    case '"':
    case '<':
      return r_err_char(reader, "IRI", c);
    case '>':
      return SERD_SUCCESS;
    case '\\':
      if (read_UCHAR(reader, *dest, &code)) {
        return r_err_char(reader, "IRI escape", c);
      }

      if (code == ' ' || code == '<' || code == '>') {
        return r_err(reader,
                     SERD_BAD_SYNTAX,
                     "invalid escaped IRI character U+%04X",
                     code);
      }
      break;
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      return r_err_char(reader, "IRI", c);
    default:
      if (c <= 0) {
        st = r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
      } else if (c <= 0x20) {
        st = r_err(reader,
                   SERD_BAD_SYNTAX,
                   "invalid IRI character (escape %%%02X)",
                   (unsigned)c);
        if (!reader->strict) {
          if (!(st = push_byte(reader, *dest, c))) {
            st = SERD_FAILURE;
          }
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
read_PrefixedName(SerdReader* const  reader,
                  TokenHeader* const dest,
                  bool* const        ate_dot)
{
  SerdStatus st = SERD_SUCCESS;
  TRY_FAILING(st, read_PN_PREFIX(reader, dest, ate_dot));

  if (peek_byte(reader) != ':') {
    return SERD_FAILURE;
  }

  TRY(st, push_byte(reader, dest, eat_byte_safe(reader, ':')));
  TRY_FAILING(st, read_PN_LOCAL(reader, dest, ate_dot));
  return SERD_SUCCESS;
}

static SerdStatus
read_0_9(SerdReader* const  reader,
         TokenHeader* const str,
         const bool         at_least_one)
{
  unsigned   count = 0;
  SerdStatus st    = SERD_SUCCESS;
  for (int c = 0; is_digit((c = peek_byte(reader))); ++count) {
    TRY(st, push_byte(reader, str, eat_byte_safe(reader, c)));
  }

  if (at_least_one && count == 0) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected digit");
  }

  return st;
}

static SerdStatus
read_number(SerdReader* const   reader,
            TokenHeader** const dest,
            TokenHeader** const datatype,
            bool* const         ate_dot)
{
  if (!(*dest = push_node_head(reader, SERD_LITERAL))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st = SERD_SUCCESS;
  int        c  = peek_byte(reader);
  if (c == '-' || c == '+') {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
  }

  bool has_decimal = false;
  if ((c = peek_byte(reader)) == '.') {
    has_decimal = true;
    // decimal case 2 (e.g. ".0" or "-.0" or "+.0")
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    TRY(st, read_0_9(reader, *dest, true));
  } else {
    // all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
    TRY(st, read_0_9(reader, *dest, true));
    if ((c = peek_byte(reader)) == '.') {
      // Annoyingly, dot can be end of statement, so tentatively eat
      skip_byte(reader, c);
      c = peek_byte(reader);
      if (!is_digit(c) && c != 'e' && c != 'E') {
        *ate_dot = true; // Force caller to deal with silly grammar
      } else {
        has_decimal = true;
        TRY(st, push_byte(reader, *dest, '.'));
        read_0_9(reader, *dest, false);
      }
    }
  }

  c = peek_byte(reader);
  if (c == 'e' || c == 'E') {
    // double
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    c = peek_byte(reader);
    if (c == '+' || c == '-') {
      TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    }
    TRY(st, read_0_9(reader, *dest, true));
    *datatype = push_node(reader, SERD_URI, serd_symbols[XSD_DOUBLE]);
  } else if (has_decimal) {
    *datatype = push_node(reader, SERD_URI, serd_symbols[XSD_DECIMAL]);
  } else {
    *datatype = push_node(reader, SERD_URI, serd_symbols[XSD_INTEGER]);
  }

  (*dest)->flags |= SERD_HAS_DATATYPE;
  return *datatype ? SERD_SUCCESS : SERD_BAD_STACK;
}

static SerdStatus
read_iri(SerdReader* const   reader,
         TokenHeader** const dest,
         bool* const         ate_dot)
{
  if (peek_byte(reader) == '<') {
    return read_IRIREF(reader, dest);
  }

  *dest = push_node_head(reader, SERD_CURIE);
  return *dest ? read_PrefixedName(reader, *dest, ate_dot) : SERD_BAD_STACK;
}

static SerdStatus
read_literal(SerdReader* const   reader,
             TokenHeader** const dest,
             TokenHeader** const meta,
             bool* const         ate_dot)
{
  if (!(*dest = push_node_head(reader, SERD_LITERAL))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st = SERD_SUCCESS;
  if ((st = read_String(reader, *dest))) {
    return st;
  }

  const int next = peek_byte(reader);
  if (next == '@') {
    skip_byte(reader, '@');
    (*dest)->flags |= SERD_HAS_LANGUAGE;
    st = read_LANGTAG(reader, meta);
  } else if (next == '^') {
    skip_byte(reader, '^');
    TRY(st, eat_byte_check(reader, '^'));
    (*dest)->flags |= SERD_HAS_DATATYPE;
    st = read_iri(reader, meta, ate_dot);
  }

  return st;
}

static SerdStatus
read_verb(SerdReader* const reader, TokenHeader** const dest)
{
  if (peek_byte(reader) == '<') {
    return read_IRIREF(reader, dest);
  }

  const size_t orig_stack_size = reader->stack.size;

  TokenHeader* node = NULL;
  if (!(node = push_node_head(reader, SERD_CURIE))) {
    return SERD_BAD_STACK;
  }

  // Try to read as a prefixed name
  bool       ate_dot = false;
  SerdStatus st      = read_PrefixedName(reader, node, &ate_dot);

  if (st == SERD_FAILURE) {
    // Check if this is actually the "a" shorthand
    const char* const str = (const char*)(node + 1U);
    const size_t      len = node->length;
    if (len == 1 && str[0] == 'a') {
      node = push_node(reader, SERD_URI, serd_symbols[RDF_TYPE]);
      st   = node ? SERD_SUCCESS : SERD_BAD_STACK;
    } else {
      st = SERD_BAD_SYNTAX;
    }
  }

  if (st) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);
    *dest = NULL;
    return r_err(reader, st, "bad verb");
  }

  *dest = node;
  return SERD_SUCCESS;
}

static SerdStatus
read_BLANK_NODE_LABEL(SerdReader* const   reader,
                      TokenHeader** const dest,
                      bool* const         ate_dot)
{
  SerdStatus st = SERD_SUCCESS;

  skip_byte(reader, '_');
  TRY(st, eat_byte_check(reader, ':'));

  TokenHeader* const n = *dest = push_node_head(reader, SERD_BLANK);
  if (!n) {
    return SERD_BAD_STACK;
  }

  if (reader->bprefix) {
    TRY(st,
        push_bytes(reader,
                   n,
                   (const uint8_t*)reader->bprefix,
                   (unsigned)reader->bprefix_len));
  }

  int c = peek_byte(reader); // First: (PN_CHARS | '_' | [0-9])
  if (is_digit(c) || c == '_') {
    TRY(st, push_byte(reader, n, eat_byte_safe(reader, c)));
  } else if ((st = read_PN_CHARS(reader, n))) {
    st = st > SERD_FAILURE ? st : SERD_BAD_SYNTAX;
    return r_err(reader, st, "invalid name start");
  }

  while (!st && (c = peek_byte(reader)) > 0) { // Middle: (PN_CHARS | '.')*
    st = (c == '.') ? push_byte(reader, n, eat_byte_safe(reader, c))
                    : read_PN_CHARS(reader, n);
  }

  if (st > SERD_FAILURE) {
    return st;
  }

  char* const buf = (char*)(n + 1U);
  if (n->length && buf[n->length - 1] == '.' && read_PN_CHARS(reader, n)) {
    // Ate trailing dot, pop it from stack/node and inform caller
    *ate_dot = pop_last_node_char(reader, n);
  }

  if (fancy_syntax(reader)) {
    if (is_digit(buf[reader->bprefix_len + 1])) {
      if ((buf[reader->bprefix_len]) == 'b') {
        buf[reader->bprefix_len] = 'B'; // Prevent clash
        reader->seen_genid       = true;
      } else if (reader->seen_genid && buf[reader->bprefix_len] == 'B') {
        return r_err(reader,
                     SERD_BAD_LABEL,
                     "found both 'b' and 'B' blank IDs, prefix required");
      }
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

static SerdStatus
read_anon(SerdReader* const   reader,
          ReadContext         ctx,
          const bool          subject,
          TokenHeader** const dest)
{
  skip_byte(reader, '[');

  const SerdEventFlags old_flags = *ctx.flags;
  const bool           empty     = peek_delim(reader, ']');

  if (subject) {
    *ctx.flags |= empty ? SERD_EMPTY_S : SERD_ANON_S;
  } else {
    *ctx.flags |= empty ? SERD_EMPTY_O : SERD_ANON_O;
  }

  if (!*dest) {
    if (!(*dest = blank_id(reader))) {
      return SERD_BAD_STACK;
    }
  }

  // Emit statement with this anonymous object first
  SerdStatus st = SERD_SUCCESS;
  if (ctx.subject) {
    TRY(st, emit_statement(reader, ctx, *dest, NULL));
  }

  // Switch the subject to the anonymous node and read its description
  ctx.subject = *dest;
  if (!empty) {
    bool ate_dot_in_list = false;
    TRY(st, read_predicateObjectList(reader, ctx, &ate_dot_in_list));

    if (ate_dot_in_list) {
      return r_err(reader, SERD_BAD_SYNTAX, "'.' inside blank");
    }

    read_ws_star(reader);
    *ctx.flags = old_flags;

    st = emit_event(reader, serd_end_event(stack_token_view(*dest).string));
  }

  return st > SERD_FAILURE ? st : eat_byte_check(reader, ']');
}

// Read a "named" object: a boolean literal or a prefixed name
static SerdStatus
read_named_object(SerdReader* const   reader,
                  TokenHeader** const dest,
                  TokenHeader** const datatype,
                  bool* const         ate_dot)
{
  TokenHeader* node = NULL;
  if (!(node = push_node_head(reader, SERD_CURIE))) {
    return SERD_BAD_STACK;
  }

  // Try to read as a prefixed name
  SerdStatus st = read_PrefixedName(reader, node, ate_dot);

  if (st == SERD_FAILURE) {
    // Check if this is actually a boolean literal
    const char* const str = (const char*)(node + 1U);
    if ((node->length == 4 && !memcmp(str, "true", 4)) ||
        (node->length == 5 && !memcmp(str, "false", 5))) {
      if (!(*datatype =
              push_node(reader, SERD_URI, serd_symbols[XSD_BOOLEAN]))) {
        st = SERD_BAD_STACK;
      } else {
        node->type = SERD_LITERAL;
        node->flags |= SERD_HAS_DATATYPE;
        st = SERD_SUCCESS;
      }
    }
  }

  st = (st == SERD_FAILURE) ? SERD_BAD_SYNTAX : st;
  if (st) {
    return r_err(reader, st, "expected prefixed name (%s)", serd_strerror(st));
  }

  *dest = node;
  return SERD_SUCCESS;
}

/* If emit is true: recurses, calling statement_sink for every statement
   encountered, and leaves stack in original calling state (i.e. pops
   everything it pushes). */
static SerdStatus
read_object(SerdReader* const        reader,
            const ReadContext* const ctx,
            TokenHeader** const      o,
            TokenHeader** const      meta,
            const bool               emit,
            bool* const              ate_dot)
{
  const size_t orig_stack_size = reader->stack.size;

  SerdStatus st = SERD_FAILURE;

  *o = *meta = NULL;

  bool      simple = (ctx->subject != 0);
  const int c      = peek_byte(reader);
  if (!fancy_syntax(reader)) {
    if (c != '"' && c != ':' && c != '<' && c != '_') {
      return r_err(reader, SERD_BAD_SYNTAX, "expected: ':', '<', or '_'");
    }
  }

  switch (c) {
  case EOF:
  case ')':
    return r_err(reader, SERD_BAD_SYNTAX, "expected object");
  case '[':
    simple = false;
    st     = read_anon(reader, *ctx, false, o);
    break;
  case '(':
    simple = false;
    st     = read_collection(reader, *ctx, o);
    break;
  case '_':
    st = read_BLANK_NODE_LABEL(reader, o, ate_dot);
    break;
  case '<':
    st = read_IRIREF(reader, o);
    break;
  case ':':
    st = read_iri(reader, o, ate_dot);
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
    st = read_number(reader, o, meta, ate_dot);
    break;
  case '\"':
  case '\'':
    st = read_literal(reader, o, meta, ate_dot);
    break;
  default:
    // Either a boolean literal or a prefixed name
    st = read_named_object(reader, o, meta, ate_dot);
  }

  if (!st && emit && simple) {
    st = emit_statement(reader, *ctx, *o, *meta);
  } else if (!st && !emit) {
    return SERD_SUCCESS;
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);
  return st;
}

static SerdStatus
read_objectList(SerdReader* const reader, ReadContext ctx, bool* const ate_dot)
{
  SerdStatus st = SERD_SUCCESS;

  TokenHeader* object = NULL;
  TokenHeader* meta   = NULL;
  TRY(st, read_object(reader, &ctx, &object, &meta, true, ate_dot));
  if (!fancy_syntax(reader) && peek_delim(reader, ',')) {
    return r_err(
      reader, SERD_BAD_SYNTAX, "syntax does not support abbreviation");
  }

  while (st <= SERD_FAILURE && !*ate_dot && eat_delim(reader, ',')) {
    st = read_object(reader, &ctx, &object, &meta, true, ate_dot);
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
         !(st = read_objectList(reader, ctx, ate_dot)) && !*ate_dot) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);

    bool ate_semi = false;
    int  c        = 0;
    do {
      read_ws_star(reader);
      c = peek_byte(reader);
      if (c < 0) {
        return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
      }

      if (c == '.' || c == ']' || c == '}') {
        return SERD_SUCCESS;
      }

      if (c == ';') {
        skip_byte(reader, c);
        ate_semi = true;
      }
    } while (c == ';');

    if (!ate_semi) {
      return r_err(reader, SERD_BAD_SYNTAX, "missing ';' or '.'");
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
read_collection(SerdReader* const   reader,
                ReadContext         ctx,
                TokenHeader** const dest)
{
  SerdStatus st = SERD_SUCCESS;

  skip_byte(reader, '(');

  bool end = peek_delim(reader, ')');
  if (!(*dest = end ? reader->rdf_nil : blank_id(reader))) {
    return SERD_BAD_STACK;
  }

  if (ctx.subject) { // Reading a collection object
    *ctx.flags |= (end ? 0 : SERD_LIST_O);
    TRY(st, emit_statement(reader, ctx, *dest, NULL));
    *ctx.flags &= (SerdEventFlags) ~((unsigned)SERD_LIST_O);
  } else { // Reading a collection subject
    *ctx.flags |= (end ? 0 : SERD_LIST_S);
  }

  if (end) {
    return end_collection(reader, st);
  }

  /* The order of node allocation here is necessarily not in stack order,
     so we create two nodes and recycle them throughout. */
  TokenHeader* const n1 =
    push_node_space(reader, SERD_BLANK, genid_size(reader));
  if (!n1) {
    return SERD_BAD_STACK;
  }

  ctx.subject = *dest;

  TokenHeader* node   = n1;
  TokenHeader* rest   = NULL;
  TokenHeader* object = NULL;
  TokenHeader* meta   = NULL;
  while (!peek_delim(reader, ')')) {
    // _:node rdf:first object
    ctx.predicate = reader->rdf_first;
    bool ate_dot  = false;
    if ((st = read_object(reader, &ctx, &object, &meta, true, &ate_dot)) ||
        ate_dot) {
      return end_collection(reader, st);
    }

    if (!(end = peek_delim(reader, ')'))) {
      /* Give rest a new ID.  Done as late as possible to ensure it is
         used and > IDs generated by read_object above. */
      if (!rest) {
        rest = blank_id(reader); // First pass, push
        assert(rest);            // Can't overflow since read_object() popped
      } else {
        set_blank_id(reader, rest, genid_size(reader));
      }
    }

    // _:node rdf:rest _:rest
    ctx.predicate = reader->rdf_rest;
    st = emit_statement(reader, ctx, (end ? reader->rdf_nil : rest), NULL);
    if (st) {
      break;
    }

    ctx.subject = rest;        // _:node = _:rest
    rest        = node;        // _:rest = (old)_:node
    node        = ctx.subject; // invariant
  }

  return end_collection(reader, st);
}

static SerdStatus
read_subject(SerdReader* const   reader,
             ReadContext         ctx,
             TokenHeader** const dest,
             int* const          s_type)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;
  switch ((*s_type = peek_byte(reader))) {
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
    return r_err(reader, SERD_BAD_SYNTAX, "subject ends with '.'");
  }

  return st;
}

static SerdStatus
read_labelOrSubject(SerdReader* const reader, TokenHeader** const dest)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;

  switch (peek_byte(reader)) {
  case '[':
    skip_byte(reader, '[');
    read_ws_star(reader);
    TRY(st, eat_byte_check(reader, ']'));
    *dest = blank_id(reader);
    return *dest ? SERD_SUCCESS : SERD_BAD_STACK;
  case '_':
    return read_BLANK_NODE_LABEL(reader, dest, &ate_dot);
  default:
    if (!read_iri(reader, dest, &ate_dot)) {
      return SERD_SUCCESS;
    } else {
      return r_err(reader, SERD_BAD_SYNTAX, "expected label or subject");
    }
  }
}

static SerdStatus
read_triples(SerdReader* const reader, ReadContext ctx, bool* const ate_dot)
{
  SerdStatus st = SERD_FAILURE;
  if (ctx.subject) {
    read_ws_star(reader);
    const int c = peek_byte(reader);
    if (c == '.') {
      *ate_dot = eat_byte_safe(reader, '.');
      return SERD_FAILURE;
    }

    if (c == '}') {
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

  TokenHeader* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));
  TRY(st, emit_event(reader, serd_base_event(stack_token_view(uri).string)));

  read_ws_star(reader);
  if (!sparql) {
    return eat_byte_check(reader, '.');
  }

  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_BAD_SYNTAX, "full stop after SPARQL BASE");
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
  TokenHeader* const name = push_node_head(reader, SERD_LITERAL);
  if (!name) {
    return SERD_BAD_STACK;
  }

  bool ate_dot = false;
  TRY_FAILING(st, read_PN_PREFIX(reader, name, &ate_dot));
  if (ate_dot || (st = eat_byte_check(reader, ':'))) {
    return r_err(reader,
                 st > SERD_FAILURE ? st : SERD_BAD_SYNTAX,
                 "expected a prefix name");
  }

  read_ws_star(reader);

  TokenHeader* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));

  st = emit_event(reader,
                  serd_prefix_event(stack_token_view(name).string,
                                    stack_token_view(uri).string));

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
    const int next = peek_byte(reader);
    if (next == 'B' || next == 'P') {
      return r_err(reader, SERD_BAD_SYNTAX, "uppercase directive");
    }
  }

  const int next = peek_byte(reader);

  if (next == 'B' || next == 'b') {
    return read_base(reader, sparql, true);
  }

  if (next == 'P' || next == 'p') {
    return read_prefixID(reader, sparql, true);
  }

  return r_err(reader, SERD_BAD_SYNTAX, "invalid directive");
}

static SerdStatus
read_wrappedGraph(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, eat_byte_check(reader, '{'));
  read_ws_star(reader);

  while (peek_byte(reader) != '}') {
    const size_t orig_stack_size = reader->stack.size;
    bool         ate_dot         = false;
    int          s_type          = 0;

    ctx->subject = 0;
    if ((st = read_subject(reader, *ctx, &ctx->subject, &s_type))) {
      return r_err(reader, st, "expected subject");
    }

    if ((st = read_triples(reader, *ctx, &ate_dot)) && s_type != '[') {
      return r_err(reader, st, "bad predicate object list");
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
    return r_err(reader, SERD_BAD_SYNTAX, "graph followed by '.'");
  }

  return SERD_SUCCESS;
}

static bool
token_equals(const TokenHeader* const node, const ZixStringView tok)
{
  if (node->length != tok.length) {
    return false;
  }

  const char* const node_string = (const char*)(node + 1U);
  for (size_t i = 0U; i < tok.length; ++i) {
    if (serd_to_upper(node_string[i]) != serd_to_upper(tok.data[i])) {
      return false;
    }
  }

  return true;
}

SerdStatus
read_n3_statement(SerdReader* const reader)
{
  static const ZixStringView base_token   = ZIX_STATIC_STRING("base");
  static const ZixStringView false_token  = ZIX_STATIC_STRING("false");
  static const ZixStringView graph_token  = ZIX_STATIC_STRING("graph");
  static const ZixStringView prefix_token = ZIX_STATIC_STRING("prefix");
  static const ZixStringView true_token   = ZIX_STATIC_STRING("true");

  SerdEventFlags flags   = 0U;
  ReadContext    ctx     = {NULL, NULL, NULL, &flags};
  bool           ate_dot = false;
  int            s_type  = 0;
  SerdStatus     st      = SERD_SUCCESS;
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
        reader, SERD_BAD_SYNTAX, "syntax does not support directives");
    }
    TRY(st, read_directive(reader));
    read_ws_star(reader);
    break;
  case '{':
    if (reader->syntax == SERD_TRIG) {
      TRY(st, read_wrappedGraph(reader, &ctx));
      read_ws_star(reader);
    } else {
      return r_err(reader, SERD_BAD_SYNTAX, "syntax does not support graphs");
    }
    break;
  default:
    TRY_FAILING(st, read_subject(reader, ctx, &ctx.subject, &s_type));

    if (token_equals(ctx.subject, base_token)) {
      st = read_base(reader, true, false);
    } else if (token_equals(ctx.subject, prefix_token)) {
      st = read_prefixID(reader, true, false);
    } else if (token_equals(ctx.subject, graph_token)) {
      ctx.subject = NULL;
      read_ws_star(reader);
      TRY(st, read_labelOrSubject(reader, &ctx.graph));
      read_ws_star(reader);
      TRY(st, read_wrappedGraph(reader, &ctx));
      ctx.graph = 0;
      read_ws_star(reader);
    } else if (token_equals(ctx.subject, true_token) ||
               token_equals(ctx.subject, false_token)) {
      return r_err(reader, SERD_BAD_SYNTAX, "expected subject");
    } else if (read_ws_star(reader) && peek_byte(reader) == '{') {
      if (s_type == '(' || (s_type == '[' && !*ctx.flags)) {
        return r_err(reader, SERD_BAD_SYNTAX, "invalid graph name");
      }
      ctx.graph   = ctx.subject;
      ctx.subject = NULL;
      TRY(st, read_wrappedGraph(reader, &ctx));
      read_ws_star(reader);
    } else if ((st = read_triples(reader, ctx, &ate_dot))) {
      if (st == SERD_FAILURE && s_type == '[') {
        return SERD_SUCCESS;
      }

      if (st <= SERD_FAILURE && ate_dot &&
          (reader->strict || (s_type != '('))) {
        return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of statement");
      }

      return st > SERD_FAILURE ? st : SERD_BAD_SYNTAX;
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
  assert(reader);

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
  while (!reader->source.eof) {
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
read_nquads_statement(SerdReader* const reader)
{
  SerdStatus     st      = SERD_SUCCESS;
  SerdEventFlags flags   = 0U;
  ReadContext    ctx     = {NULL, NULL, NULL, &flags};
  TokenHeader*   object  = NULL;
  TokenHeader*   meta    = NULL;
  bool           ate_dot = false;
  int            s_type  = 0;

  read_ws_star(reader);
  if (peek_byte(reader) == EOF) {
    return SERD_FAILURE;
  }

  if (peek_byte(reader) == '@') {
    return r_err(reader, SERD_BAD_SYNTAX, "syntax does not support directives");
  }

  // subject predicate object
  if ((st = read_subject(reader, ctx, &ctx.subject, &s_type)) ||
      !read_ws_star(reader) || (st = read_IRIREF(reader, &ctx.predicate)) ||
      !read_ws_star(reader) ||
      (st = read_object(reader, &ctx, &object, &meta, false, &ate_dot))) {
    return st;
  }

  if (!ate_dot) { // graphLabel?
    read_ws_star(reader);
    switch (peek_byte(reader)) {
    case '.':
      break;
    case '_':
      TRY(st, read_BLANK_NODE_LABEL(reader, &ctx.graph, &ate_dot));
      break;
    default:
      TRY(st, read_IRIREF(reader, &ctx.graph));
    }

    // Terminating '.'
    read_ws_star(reader);
    if (!ate_dot) {
      TRY(st, eat_byte_check(reader, '.'));
    }
  }

  return emit_statement(reader, ctx, object, meta);
}

SerdStatus
read_nquadsDoc(SerdReader* const reader)
{
  while (!reader->source.eof) {
    const size_t orig_stack_size = reader->stack.size;

    const SerdStatus st = read_nquads_statement(reader);
    if (st > SERD_FAILURE) {
      if (reader->strict) {
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return st;
      }
      serd_reader_skip_until_byte(reader, '\n');
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
  }

  return SERD_SUCCESS;
}

#if defined(__clang__) && __clang_major__ >= 10
_Pragma("clang diagnostic pop")
#endif
