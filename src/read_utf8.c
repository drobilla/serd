// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_utf8.h"

#include "reader_impl.h"
#include "reader_internal.h"
#include "string_utils.h"

#include <serd/reader.h>
#include <serd/status.h>

#include <stdint.h>

enum {
  MAX_UTF8_BYTES = 4U,
};

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
                             uint32_t* const   code,
                             const uint8_t     lead)
{
  SerdStatus st = SERD_SUCCESS;

  if (!(*size = utf8_num_bytes(lead))) {
    return bad_byte(reader, "UTF-8 leading", lead);
  }

  bytes[0] = lead;
  for (uint8_t i = 1U; !st && i < *size; ++i) {
    const int c = peek_byte(reader);
    if (c < 0) {
      st = r_err_eof(reader, SERD_BAD_TEXT);
    } else if (c < 0x80 || c > 0xC0) {
      st = bad_byte(reader, "UTF-8 continuation", (uint8_t)c);
    } else {
      st       = skip_byte(reader, c);
      bytes[i] = (uint8_t)c;
    }
  }

  if (!st) {
    *code = parse_counted_utf8_char(bytes, *size);
    if (*size > utf8_num_bytes_for_codepoint(*code)) {
      st = r_err(reader, SERD_BAD_TEXT, "overlong UTF-8 for U+%06X", *code);
    }

    if (*code >= 0xD800U && *code <= 0xDFFFU) {
      st = r_err(reader, SERD_BAD_TEXT, "reserved character U+%04X", *code);
    }
  }

  return st;
}

SerdStatus
read_utf8_continuation(SerdReader* const  reader,
                       TokenHeader* const dest,
                       uint32_t* const    code,
                       const uint8_t      lead)
{
  uint8_t    size                  = 0U;
  SerdStatus st                    = SERD_SUCCESS;
  uint8_t    bytes[MAX_UTF8_BYTES] = {lead, 0U, 0U, 0U};

  if ((st = read_utf8_continuation_bytes(reader, bytes, &size, code, lead))) {
    return reader->strict ? st : push_bytes(reader, dest, replacement_char, 3);
  }

  st = push_bytes(reader, dest, bytes, size);

  return st;
}
