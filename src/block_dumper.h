// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BLOCK_DUMPER_H
#define SERD_SRC_BLOCK_DUMPER_H

#include "serd/attributes.h"
#include "serd/output_stream.h"
#include "serd/status.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  SerdOutputStream* SERD_ALLOCATED out;        ///< Output stream to write to
  char* SERD_ALLOCATED             buf;        ///< Local buffer if needed
  size_t                           size;       ///< Bytes pending for this block
  size_t                           block_size; ///< Block size to write in bytes
} SerdBlockDumper;

/**
   Set up a new output stream wrapper that writes in blocks.

   This allocates a buffer internally, which must be eventually freed by
   calling serd_block_dumper_close().
*/
SerdStatus
serd_block_dumper_open(SerdBlockDumper* SERD_NONNULL  dumper,
                       SerdOutputStream* SERD_NONNULL output,
                       size_t                         block_size);

void
serd_block_dumper_flush(SerdBlockDumper* SERD_NONNULL dumper);

void
serd_block_dumper_close(SerdBlockDumper* SERD_NONNULL dumper);

/**
   Write some bytes to the page writer.

   This works like any other SerdWriteFunc, but will append to an internal
   buffer and only actually write to the output when a whole block is ready.
*/
static inline size_t
serd_block_dumper_write(const void* SERD_NONNULL            buf,
                        const size_t                        size,
                        const size_t                        nmemb,
                        SerdBlockDumper* SERD_NONNULL const dumper)
{
  if (dumper->block_size == 1) {
    return dumper->out->write(buf, size, nmemb, dumper->out->stream);
  }

  size_t       len      = size * nmemb;
  const size_t orig_len = len;
  while (len) {
    const size_t space = dumper->block_size - dumper->size;
    const size_t n     = space < len ? space : len;

    // Write as much as possible into the remaining buffer space
    memcpy(dumper->buf + dumper->size, buf, n);
    dumper->size += n;
    buf = (const char*)buf + n;
    len -= n;

    // Flush page if buffer is full
    if (dumper->size == dumper->block_size) {
      dumper->out->write(
        dumper->buf, 1, dumper->block_size, dumper->out->stream);
      dumper->size = 0;
    }
  }

  return orig_len;
}

#endif // SERD_SRC_DUMPER_H
