// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BYTE_SOURCE_H
#define SERD_SRC_BYTE_SOURCE_H

#include "caret.h" // IWYU pragma: keep

#include "serd/caret.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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

SerdStatus
serd_byte_source_init(ZixAllocator*    allocator,
                      SerdByteSource*  source,
                      SerdInputStream* input,
                      const SerdNode*  name,
                      size_t           block_size);

void
serd_byte_source_destroy(ZixAllocator* allocator, SerdByteSource* source);

SerdStatus
serd_byte_source_prepare(SerdByteSource* source);

SerdStatus
serd_byte_source_page(SerdByteSource* source);

SerdStatus
serd_byte_source_skip_bom(SerdByteSource* source);

ZIX_PURE_FUNC static inline int
serd_byte_source_peek(const SerdByteSource* const source)
{
  assert(source->prepared);

  return source->eof ? EOF : (int)source->read_buf[source->read_head];
}

static inline SerdStatus
serd_byte_source_advance_past(SerdByteSource* const source, const int current)
{
  /* Reading the buffer here can be an expensive cache miss, so we only assert
     that the passed current character is correct in debug builds.  In release
     builds, this function only accesses the `source` structure, unless a page
     read needs to happen. */

  assert(current == serd_byte_source_peek(source));

  if (current == '\n') {
    ++source->caret.line;
    source->caret.col = 0;
  } else {
    ++source->caret.col;
  }

  SerdStatus st = SERD_SUCCESS;
  if (++source->read_head >= source->buf_size) {
    st = serd_byte_source_page(source);
  }

  return st;
}

#endif // SERD_SRC_BYTE_SOURCE_H
