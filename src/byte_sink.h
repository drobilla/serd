// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BYTE_SINK_H
#define SERD_SRC_BYTE_SINK_H

#include "serd_internal.h"
#include "system.h"

#include "serd/status.h"
#include "serd/stream.h"

#include <stddef.h>
#include <string.h>

typedef struct SerdByteSinkImpl {
  SerdWriteFunc sink;
  void*         stream;
  char*         buf;
  size_t        size;
  size_t        block_size;
} SerdByteSink;

static inline SerdByteSink
serd_byte_sink_new(SerdWriteFunc sink, void* stream, size_t block_size)
{
  SerdByteSink bsink;
  bsink.sink       = sink;
  bsink.stream     = stream;
  bsink.size       = 0;
  bsink.block_size = block_size;
  bsink.buf =
    ((block_size > 1) ? (char*)serd_allocate_buffer(block_size) : NULL);
  return bsink;
}

static inline SerdStatus
serd_byte_sink_flush(SerdByteSink* bsink)
{
  if (bsink->block_size > 1 && bsink->size > 0) {
    const size_t size  = bsink->size;
    const size_t n_out = bsink->sink(bsink->buf, size, bsink->stream);
    bsink->size        = 0;

    return (n_out != size) ? SERD_BAD_WRITE : SERD_SUCCESS;
  }

  return SERD_SUCCESS;
}

static inline void
serd_byte_sink_free(SerdByteSink* bsink)
{
  serd_byte_sink_flush(bsink);
  serd_free_aligned(bsink->buf);
  bsink->buf = NULL;
}

static inline size_t
serd_byte_sink_write(const void* buf, size_t len, SerdByteSink* bsink)
{
  if (len == 0) {
    return 0;
  }

  if (bsink->block_size == 1) {
    return bsink->sink(buf, len, bsink->stream);
  }

  const size_t orig_len = len;
  while (len) {
    const size_t space = bsink->block_size - bsink->size;
    const size_t n     = MIN(space, len);

    // Write as much as possible into the remaining buffer space
    memcpy(bsink->buf + bsink->size, buf, n);
    bsink->size += n;
    buf = (const char*)buf + n;
    len -= n;

    // Flush page if buffer is full
    if (bsink->size == bsink->block_size) {
      bsink->sink(bsink->buf, bsink->block_size, bsink->stream);
      bsink->size = 0;
    }
  }
  return orig_len;
}

#endif // SERD_SRC_BYTE_SINK_H
