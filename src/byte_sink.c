// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd_internal.h"
#include "system.h"

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct SerdByteSinkImpl {
  SerdWriteFunc sink;
  void*         stream;
  char*         buf;
  size_t        size;
  size_t        block_size;
};

SerdByteSink*
serd_byte_sink_new(SerdWriteFunc write_func, void* stream, size_t block_size)
{
  SerdByteSink* sink = (SerdByteSink*)calloc(1, sizeof(SerdByteSink));

  sink->sink       = write_func;
  sink->stream     = stream;
  sink->block_size = block_size;

  if (block_size > 1) {
    sink->buf = (char*)serd_allocate_buffer(block_size);
  }

  return sink;
}

size_t
serd_byte_sink_write(const void*   buf,
                     size_t        size,
                     size_t        nmemb,
                     SerdByteSink* sink)
{
  assert(size == 1);
  (void)size;

  if (nmemb == 0) {
    return 0;
  }

  if (sink->block_size == 1) {
    return sink->sink(buf, 1, nmemb, sink->stream);
  }

  const size_t orig_len = nmemb;
  while (nmemb) {
    const size_t space = sink->block_size - sink->size;
    const size_t n     = MIN(space, nmemb);

    // Write as much as possible into the remaining buffer space
    memcpy(sink->buf + sink->size, buf, n);
    sink->size += n;
    buf = (const char*)buf + n;
    nmemb -= n;

    // Flush page if buffer is full
    if (sink->size == sink->block_size) {
      sink->sink(sink->buf, 1, sink->block_size, sink->stream);
      sink->size = 0;
    }
  }

  return orig_len;
}

void
serd_byte_sink_flush(SerdByteSink* sink)
{
  if (sink->block_size > 1 && sink->size > 0) {
    sink->sink(sink->buf, 1, sink->size, sink->stream);
    sink->size = 0;
  }
}

void
serd_byte_sink_free(SerdByteSink* const sink)
{
  if (sink) {
    serd_byte_sink_flush(sink);
    serd_free_aligned(sink->buf);
    free(sink);
  }
}
