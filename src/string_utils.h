// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STRING_UTILS_H
#define SERD_SRC_STRING_UTILS_H

#include <serd/serd.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Unicode replacement character in UTF-8 */
static const uint8_t replacement_char[] = {0xEF, 0xBF, 0xBD};

/** Return true if `c` lies within [`min`...`max`] (inclusive) */
static inline bool
in_range(const int c, const int min, const int max)
{
  return (c >= min && c <= max);
}

/** RFC2234: ALPHA ::= %x41-5A / %x61-7A  ; A-Z / a-z */
static inline bool
is_alpha(const int c)
{
  return in_range(c, 'A', 'Z') || in_range(c, 'a', 'z');
}

/** RFC2234: DIGIT ::= %x30-39  ; 0-9 */
static inline bool
is_digit(const int c)
{
  return in_range(c, '0', '9');
}

/** RFC2234: HEXDIG ::= DIGIT / "A" / "B" / "C" / "D" / "E" / "F" */
static inline bool
is_hexdig(const int c)
{
  return is_digit(c) || in_range(c, 'A', 'F');
}

/** Turtle / JSON / C: XDIGIT ::= DIGIT / A-F / a-f */
static inline bool
is_xdigit(const int c)
{
  return is_hexdig(c) || in_range(c, 'a', 'f');
}

static inline bool
is_space(const char c)
{
  return c == ' ' || in_range(c, '\t', '\r');
}

static inline bool
is_print(const int c)
{
  return in_range(c, 0x20, 0x7E);
}

static inline bool
is_base64(const int c)
{
  return is_alpha(c) || is_digit(c) || c == '+' || c == '/' || c == '=';
}

static inline bool
is_windows_path(const uint8_t* path)
{
  return is_alpha(path[0]) && (path[1] == ':' || path[1] == '|') &&
         (path[2] == '/' || path[2] == '\\');
}

size_t
serd_substrlen(const uint8_t* str,
               size_t         len,
               size_t*        n_bytes,
               SerdNodeFlags* flags);

static inline uint8_t
hex_digit_value(const uint8_t c)
{
  return (uint8_t)((c > '9') ? ((c & ~0x20) - 'A' + 10) : (c - '0'));
}

static inline char
serd_to_upper(const char c)
{
  return (char)(in_range(c, 'a', 'z') ? (c - 32) : c);
}

SERD_PURE_FUNC static inline int
serd_strcasecmp(const char* s1, const char* s2)
{
  while (*s1 && *s2) {
    const char c1 = serd_to_upper(*s1++);
    const char c2 = serd_to_upper(*s2++);
    if (c1 != c2) {
      return (c1 < c2) ? -1 : +1;
    }
  }

  const char c1 = serd_to_upper(*s1);
  const char c2 = serd_to_upper(*s2);
  return (c1 == c2) ? 0 : (c1 < c2) ? -1 : +1;
}

static inline uint8_t
utf8_num_bytes(const uint8_t leading)
{
  return ((leading & 0x80U) == 0x00U)   ? 1U  // Starts with `0'
         : ((leading & 0xE0U) == 0xC0U) ? 2U  // Starts with `110'
         : ((leading & 0xF0U) == 0xE0U) ? 3U  // Starts with `1110'
         : ((leading & 0xF8U) == 0xF0U) ? 4U  // Starts with `11110'
                                        : 0U; // Invalid
}

/// Return the code point of a UTF-8 character with known length
static inline uint32_t
parse_counted_utf8_char(const uint8_t* const utf8, const uint8_t size)
{
  uint32_t c = utf8[0] & ((1U << (8U - size)) - 1U);
  for (size_t i = 1; i < size; ++i) {
    c = (c << 6U) | (utf8[i] & 0x3FU);
  }
  return c;
}

/// Parse a UTF-8 character, set *size to the length, and return the code point
static inline uint32_t
parse_utf8_char(const uint8_t* const utf8, uint8_t* const size)
{
  switch (*size = utf8_num_bytes(utf8[0])) {
  case 1:
  case 2:
  case 3:
  case 4:
    return parse_counted_utf8_char(utf8, *size);
  default:
    *size = 0;
    return 0U;
  }
}

#endif // SERD_SRC_STRING_UTILS_H
