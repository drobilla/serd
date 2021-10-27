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

#ifndef SERD_BYTE_SOURCE_H
#define SERD_BYTE_SOURCE_H

#include "caret.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  SerdInputStream* in;         ///< Input stream to read from
  size_t           block_size; ///< Number of bytes to read at a time
  size_t           buf_size;   ///< Number of bytes in block
  SerdNode*        name;       ///< Name of stream (for caret)
  SerdCaret        caret;      ///< File position for error reporting
  uint8_t*         block;      ///< Buffer if reading blocks
  const uint8_t*   read_buf;   ///< Pointer to block or read_byte
  size_t           read_head;  ///< Offset into read_buf
  uint8_t          read_byte;  ///< 1-byte 'buffer' if reading bytes
  bool             prepared;   ///< True iff prepared for reading
  bool             eof;        ///< True iff end of file reached
} SerdByteSource;

SerdByteSource*
serd_byte_source_new_input(SerdAllocator*   allocator,
                           SerdInputStream* input,
                           const SerdNode*  name,
                           size_t           block_size);

void
serd_byte_source_free(SerdAllocator* allocator, SerdByteSource* source);

SerdStatus
serd_byte_source_prepare(SerdByteSource* source);

SerdStatus
serd_byte_source_page(SerdByteSource* source);

SERD_PURE_FUNC
static inline uint8_t
serd_byte_source_peek(SerdByteSource* source)
{
  assert(source->prepared);
  return source->read_buf[source->read_head];
}

static inline SerdStatus
serd_byte_source_advance(SerdByteSource* source)
{
  SerdStatus st      = SERD_SUCCESS;
  const bool was_eof = source->eof;

  switch (serd_byte_source_peek(source)) {
  case '\0':
    break;
  case '\n':
    ++source->caret.line;
    source->caret.col = 0;
    break;
  default:
    ++source->caret.col;
  }

  if (++source->read_head >= source->buf_size) {
    st = serd_byte_source_page(source);
  }

  return (was_eof && source->eof) ? SERD_FAILURE : st;
}

#endif // SERD_BYTE_SOURCE_H
