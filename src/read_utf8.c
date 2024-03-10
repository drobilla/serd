// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_utf8.h"

#include "reader_impl.h"
#include "reader_internal.h"
#include "string_utils.h"

#define MAX_UTF8_BYTES 4U

static SerdStatus
read_utf8_continuation_bytes(SerdReader* const reader,
                             uint8_t           bytes[MAX_UTF8_BYTES],
                             uint8_t* const    size,
                             const uint8_t     lead)
{
  SerdStatus st = SERD_SUCCESS;

  if (!(*size = utf8_num_bytes(lead))) {
    return r_err(reader, SERD_BAD_TEXT, "bad UTF-8 lead 0x%X", lead);
  }

  bytes[0] = lead;
  for (uint8_t i = 1U; !st && i < *size; ++i) {
    const int c = peek_byte(reader);
    if (c < 0x80 || c > 0xC0) {
      return r_err_char(reader, "UTF-8 continuation", c);
    }

    st       = skip_byte(reader, c);
    bytes[i] = (uint8_t)c;
  }

  return st;
}

SerdStatus
read_utf8_continuation(SerdReader* const reader,
                       SerdNode* const   dest,
                       const uint8_t     lead)
{
  uint8_t size                  = 0;
  uint8_t bytes[MAX_UTF8_BYTES] = {lead, 0U, 0U, 0U};

  SerdStatus st = read_utf8_continuation_bytes(reader, bytes, &size, lead);
  if (st) {
    return reader->strict ? st : push_bytes(reader, dest, replacement_char, 3);
  }

  return push_bytes(reader, dest, bytes, size);
}

SerdStatus
read_utf8_code_point(SerdReader* const reader,
                     SerdNode* const   dest,
                     uint32_t* const   code,
                     const uint8_t     lead)
{
  uint8_t    size                  = 0U;
  uint8_t    bytes[MAX_UTF8_BYTES] = {lead, 0U, 0U, 0U};
  SerdStatus st                    = SERD_SUCCESS;

  *code = 0U;

  if (!(st = skip_byte(reader, lead))) {
    if ((st = read_utf8_continuation_bytes(reader, bytes, &size, lead))) {
      return reader->strict ? st
                            : push_bytes(reader, dest, replacement_char, 3);
    }

    if (!(st = push_bytes(reader, dest, bytes, size))) {
      *code = parse_counted_utf8_char(bytes, size);
    }
  }

  return st;
}
