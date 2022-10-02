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

SerdByteSink*
serd_byte_sink_new_buffer(SerdBuffer* const buffer)
{
  assert(buffer);

  SerdByteSink* sink = (SerdByteSink*)calloc(1, sizeof(SerdByteSink));

  sink->write_func = serd_buffer_sink;
  sink->stream     = buffer;
  sink->block_size = 1;
  sink->type       = TO_BUFFER;

  return sink;
}

static SerdByteSink*
serd_byte_sink_new_internal(const SerdWriteFunc    write_func,
                            void* const            stream,
                            const size_t           block_size,
                            const SerdByteSinkType type)
{
  SerdByteSink* sink = (SerdByteSink*)calloc(1, sizeof(SerdByteSink));

  sink->write_func = write_func;
  sink->stream     = stream;
  sink->block_size = block_size;
  sink->type       = type;

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
  posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  return serd_byte_sink_new_internal(
    (SerdWriteFunc)fwrite, file, block_size, TO_FILENAME);
}

SerdByteSink*
serd_byte_sink_new_function(const SerdWriteFunc write_func,
                            void* const         stream,
                            const size_t        block_size)
{
  assert(write_func);

  return block_size ? serd_byte_sink_new_internal(
                        write_func, stream, block_size, TO_FUNCTION)
                    : NULL;
}

void
serd_byte_sink_flush(SerdByteSink* sink)
{
  assert(sink);

  if (sink->block_size > 1 && sink->size > 0) {
    sink->write_func(sink->buf, 1, sink->size, sink->stream);
    sink->size = 0;
  }
}

SerdStatus
serd_byte_sink_close(SerdByteSink* sink)
{
  assert(sink);

  serd_byte_sink_flush(sink);

  if (sink->type == TO_FILENAME && sink->stream) {
    const int st = fclose((FILE*)sink->stream);
    sink->stream = NULL;
    return st ? SERD_ERR_UNKNOWN : SERD_SUCCESS;
  }

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
