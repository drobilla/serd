// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BYTE_SOURCE_H
#define SERD_SRC_BYTE_SOURCE_H

#include "serd/caret_view.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  SerdInputStream* in;         ///< Input stream to read from
  size_t           block_size; ///< Number of bytes to read at a time
  size_t           buf_size;   ///< Number of bytes in block
  SerdNode*        name;       ///< Name of stream (for caret)
  SerdCaretView    caret;      ///< File position for error reporting
  uint8_t*         block;      ///< Buffer if reading blocks
  const uint8_t*   read_buf;   ///< Pointer to block or read_byte
  size_t           read_head;  ///< Offset into read_buf
  uint8_t          read_byte;  ///< 1-byte 'buffer' if reading bytes
  bool             prepared;   ///< True iff prepared for reading
  bool             eof;        ///< True iff end of file reached
} SerdByteSource;

SerdByteSource*
serd_byte_source_new_input(ZixAllocator*    allocator,
                           SerdInputStream* input,
                           const SerdNode*  name,
                           size_t           block_size);

void
serd_byte_source_free(ZixAllocator* allocator, SerdByteSource* source);

SerdStatus
serd_byte_source_prepare(SerdByteSource* source);

SerdStatus
serd_byte_source_page(SerdByteSource* source);

SerdStatus
serd_byte_source_skip_bom(SerdByteSource* source);

ZIX_PURE_FUNC static inline uint8_t
serd_byte_source_peek(SerdByteSource* source)
{
  assert(source->prepared);
  return source->read_buf[source->read_head];
}

static inline SerdStatus
serd_byte_source_advance(SerdByteSource* source)
{
  SerdStatus    st      = SERD_SUCCESS;
  const bool    was_eof = source->eof;
  const uint8_t c       = serd_byte_source_peek(source);

  if (c == '\n') {
    ++source->caret.line;
    source->caret.column = 0;
  } else if (c) {
    ++source->caret.column;
  }

  if (++source->read_head >= source->buf_size) {
    st = serd_byte_source_page(source);
  }

  return (was_eof && source->eof) ? SERD_FAILURE : st;
}

#endif // SERD_SRC_BYTE_SOURCE_H
