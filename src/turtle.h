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

#ifndef SERD_TURTLE_H
#define SERD_TURTLE_H

#include "ntriples.h"
#include "string_utils.h"

#include <stdbool.h>
#include <string.h>

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
  return strchr("!#$%&\'()*+,-./;=?@_~", c) != NULL;
}

#endif // SERD_TURTLE_H
