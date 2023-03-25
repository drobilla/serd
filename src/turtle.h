// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_TURTLE_H
#define SERD_SRC_TURTLE_H

#include "ntriples.h"
#include "string_utils.h"

#include <stdbool.h>

static inline bool
is_PN_CHARS_U(const int c)
{
  return c == '_' || is_PN_CHARS_BASE(c);
}

static inline bool
is_PN_CHARS(const int c)
{
  return (is_PN_CHARS_U(c) || c == '-' || in_range(c, '0', '9') || c == 0xB7 ||
          (c >= 0x0300 && c <= 0x036F) || (c >= 0x203F && c <= 0x2040));
}

static inline bool
is_PN_LOCAL_ESC(const int c)
{
  return in_range(c, '#', '/') || (c == '!') || (c == ';') || (c == '=') ||
         (c == '?') || (c == '@') || (c == '\\') || (c == '_') || (c == '~');
}

#endif // SERD_SRC_TURTLE_H
