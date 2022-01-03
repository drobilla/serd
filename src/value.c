/*
  Copyright 2022 David Robillard <d@drobilla.net>

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

#include "serd/serd.h"

#include <stdbool.h>
#include <stdint.h>

SerdValue
serd_nothing(void)
{
  static const SerdValue value = {SERD_NOTHING, {0}};
  return value;
}

SerdValue
serd_bool(const bool v)
{
  const SerdValue value = {SERD_BOOL, {v}};
  return value;
}

SerdValue
serd_double(const double v)
{
  SerdValue value      = {SERD_DOUBLE, {0}};
  value.data.as_double = v;
  return value;
}

SerdValue
serd_float(const float v)
{
  SerdValue value     = {SERD_FLOAT, {0}};
  value.data.as_float = v;
  return value;
}

SerdValue
serd_long(const int64_t v)
{
  SerdValue value    = {SERD_LONG, {0}};
  value.data.as_long = v;
  return value;
}

SerdValue
serd_int(const int32_t v)
{
  SerdValue value   = {SERD_INT, {0}};
  value.data.as_int = v;
  return value;
}

SerdValue
serd_short(const int16_t v)
{
  SerdValue value     = {SERD_SHORT, {0}};
  value.data.as_short = v;
  return value;
}

SerdValue
serd_byte(const int8_t v)
{
  SerdValue value    = {SERD_BYTE, {0}};
  value.data.as_byte = v;
  return value;
}

SerdValue
serd_ulong(const uint64_t v)
{
  SerdValue value     = {SERD_ULONG, {0}};
  value.data.as_ulong = v;
  return value;
}

SerdValue
serd_uint(const uint32_t v)
{
  SerdValue value    = {SERD_UINT, {0}};
  value.data.as_uint = v;
  return value;
}

SerdValue
serd_ushort(const uint16_t v)
{
  SerdValue value      = {SERD_USHORT, {0}};
  value.data.as_ushort = v;
  return value;
}

SerdValue
serd_ubyte(const uint8_t v)
{
  SerdValue value     = {SERD_UBYTE, {0}};
  value.data.as_ubyte = v;
  return value;
}
