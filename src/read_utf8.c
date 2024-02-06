// Copyright 2011-2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_utf8.h"

#include "reader.h"
#include "string_utils.h"

#include <serd/reader.h>
#include <serd/status.h>

#include <stdint.h>

enum { MAX_UTF8_BYTES = 4U };

static const uint8_t replacement_char[] = {0xEFU, 0xBFU, 0xBDU};

static SerdStatus
bad_byte(SerdReader* const reader, const char* const kind, const uint8_t c)
{
  return r_err(reader, SERD_BAD_TEXT, "bad %s byte 0x%X", kind, c);
}

static SerdStatus
read_utf8_continuation_bytes(SerdReader* const reader,
                             uint8_t           bytes[MAX_UTF8_BYTES],
                             uint8_t* const    size,
                             const uint8_t     lead)
{
  SerdStatus st = SERD_SUCCESS;

  if (!(*size = utf8_num_bytes(lead))) {
    return bad_byte(reader, "UTF-8 leading", lead);
  }

  bytes[0] = lead;
  for (uint8_t i = 1U; !st && i < *size; ++i) {
    const int b = peek_byte(reader);
    if (b < 0) {
      return r_err_eof(reader, SERD_BAD_TEXT);
    }

    const uint8_t byte = (uint8_t)b;
    if (!is_utf8_continuation(byte)) {
      return bad_byte(reader, "UTF-8 continuation", byte);
    }

    st       = skip_byte(reader, b);
    bytes[i] = byte;
  }

  return st;
}

SerdStatus
read_utf8_continuation(SerdReader* const  reader,
                       TokenHeader* const dest,
                       const uint8_t      lead)
{
  uint8_t size                  = 0U;
  uint8_t bytes[MAX_UTF8_BYTES] = {lead, 0U, 0U, 0U};

  SerdStatus st = read_utf8_continuation_bytes(reader, bytes, &size, lead);
  if (st) {
    return reader->strict ? st : push_bytes(reader, dest, replacement_char, 3);
  }

  return push_bytes(reader, dest, bytes, size);
}

SerdStatus
read_utf8_code_point(SerdReader* const  reader,
                     TokenHeader* const dest,
                     uint32_t* const    code,
                     const uint8_t      lead)
{
  uint8_t    size = 0U;
  SerdStatus st   = SERD_SUCCESS;

  *code = 0U;

  if (!(st = skip_byte(reader, lead))) {
    uint8_t bytes[MAX_UTF8_BYTES] = {lead, 0U, 0U, 0U};

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
