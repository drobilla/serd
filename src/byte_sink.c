/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#define _POSIX_C_SOURCE 200809L /* for posix_fadvise and fileno */

#include "byte_sink.h"

#include "serd_config.h"
#include "system.h"

#include "serd/serd.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

SerdByteSink*
serd_byte_sink_new_buffer(SerdBuffer* const buffer)
{
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
  return block_size ? serd_byte_sink_new_internal(
                        write_func, stream, block_size, TO_FUNCTION)
                    : NULL;
}

void
serd_byte_sink_flush(SerdByteSink* sink)
{
  if (sink->block_size > 1 && sink->size > 0) {
    sink->write_func(sink->buf, 1, sink->size, sink->stream);
    sink->size = 0;
  }
}

SerdStatus
serd_byte_sink_close(SerdByteSink* sink)
{
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
    free(sink->buf);
    free(sink);
  }
}
