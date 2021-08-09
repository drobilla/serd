// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_sink.h"

#include "serd_config.h"
#include "system.h"

#include "serd/buffer.h"
#include "serd/status.h"
#include "serd/stream.h"

#include <assert.h>

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static int
close_buffer(void* const stream)
{
  serd_buffer_sink("", 1, 1, stream); // Write null terminator

  return 0;
}

SerdByteSink*
serd_byte_sink_new_buffer(SerdBuffer* const buffer)
{
  assert(buffer);

  return serd_byte_sink_new_function(
    serd_buffer_sink, close_buffer, buffer, 1U);
}

SerdByteSink*
serd_byte_sink_new_function(const SerdWriteFunc       write_func,
                            const SerdStreamCloseFunc close_func,
                            void* const               stream,
                            const size_t              block_size)
{
  if (!block_size) {
    return NULL;
  }

  SerdByteSink* sink = (SerdByteSink*)calloc(1, sizeof(SerdByteSink));

  sink->write_func = write_func;
  sink->close_func = close_func;
  sink->stream     = stream;
  sink->block_size = block_size;

  if (block_size > 1) {
    sink->buf = (char*)serd_allocate_buffer(block_size);
  }

  return sink;
}

SerdByteSink*
serd_byte_sink_new_filename(const char* const path, const size_t block_size)
{
  assert(path);

  if (!block_size) {
    return NULL;
  }

  FILE* const file = fopen(path, "wb");
  if (!file) {
    return NULL;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  (void)posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  return serd_byte_sink_new_function(
    (SerdWriteFunc)fwrite, (SerdStreamCloseFunc)fclose, file, block_size);
}

void
serd_byte_sink_flush(SerdByteSink* sink)
{
  assert(sink);

  if (sink->stream && sink->block_size > 1 && sink->size > 0) {
    sink->write_func(sink->buf, 1, sink->size, sink->stream);
    sink->size = 0;
  }
}

SerdStatus
serd_byte_sink_close(SerdByteSink* sink)
{
  assert(sink);

  serd_byte_sink_flush(sink);

  if (sink->stream && sink->close_func) {
    const int st = sink->close_func(sink->stream);
    sink->stream = NULL;
    return st ? SERD_ERR_UNKNOWN : SERD_SUCCESS;
  }

  sink->stream = NULL;
  return SERD_SUCCESS;
}

void
serd_byte_sink_free(SerdByteSink* const sink)
{
  if (sink) {
    serd_byte_sink_close(sink);
    serd_free_aligned(sink->buf);
    free(sink);
  }
}
