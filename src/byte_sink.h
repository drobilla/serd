// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BYTE_SINK_H
#define SERD_SRC_BYTE_SINK_H

#include "serd/byte_sink.h"
#include "serd/stream.h"

#include <stddef.h>
#include <string.h>

typedef enum {
  TO_BUFFER,   ///< Writing to a user-provided buffer
  TO_FILENAME, ///< Writing to a file we opened
  TO_FUNCTION, ///< Writing to a user-provided function
} SerdByteSinkType;

struct SerdByteSinkImpl {
  SerdWriteFunc    write_func; ///< User sink for TO_FUNCTION
  void*            stream;     ///< User data for write_func
  char*            buf;        ///< Local buffer iff block_size > 1
  size_t           size;       ///< Bytes written so far in this chunk
  size_t           block_size; ///< Size of chunks to write
  SerdByteSinkType type;       ///< Type of output
};

static inline size_t
serd_byte_sink_write(const void* buf, size_t len, SerdByteSink* const sink)
{
  if (len == 0) {
    return 0;
  }

  if (sink->block_size == 1) {
    return sink->write_func(buf, 1, len, sink->stream);
  }

  const size_t orig_len = len;
  while (len) {
    const size_t space = sink->block_size - sink->size;
    const size_t n     = space < len ? space : len;

    // Write as much as possible into the remaining buffer space
    memcpy(sink->buf + sink->size, buf, n);
    sink->size += n;
    buf = (const char*)buf + n;
    len -= n;

    // Flush page if buffer is full
    if (sink->size == sink->block_size) {
      sink->write_func(sink->buf, 1, sink->block_size, sink->stream);
      sink->size = 0;
    }
  }

  return orig_len;
}

#endif // SERD_SRC_BYTE_SINK_H
