// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BLOCK_DUMPER_H
#define SERD_SRC_BLOCK_DUMPER_H

#include "warnings.h"

#include "serd/output_stream.h"
#include "serd/status.h"
#include "zix/attributes.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  SerdOutputStream* ZIX_ALLOCATED out;        ///< Output stream to write to
  char* ZIX_ALLOCATED             buf;        ///< Local buffer if needed
  size_t                          size;       ///< Bytes pending for this block
  size_t                          block_size; ///< Block size to write in bytes
} SerdBlockDumper;

/**
   Set up a new output stream wrapper that writes in blocks.

   This allocates a buffer internally, which must be eventually freed by
   calling serd_block_dumper_close().
*/
SerdStatus
serd_block_dumper_open(SerdBlockDumper* ZIX_NONNULL  dumper,
                       SerdOutputStream* ZIX_NONNULL output,
                       size_t                        block_size);

/**
   Flush any pending writes.

   This should be used before closing to ensure that all of the writes have
   actually been written to the output stream.
*/
SerdStatus
serd_block_dumper_flush(SerdBlockDumper* ZIX_NONNULL dumper);

/**
   Close a block dumper.

   This frees any memory allocated when opened or written to.
*/
void
serd_block_dumper_close(SerdBlockDumper* ZIX_NONNULL dumper);

/**
   Write some bytes to the block dumper.

   This works like any other SerdWriteFunc, but will append to an internal
   buffer and only actually write to the output when a whole block is ready.
*/
static inline size_t
serd_block_dumper_write(const void* ZIX_NONNULL            buf,
                        const size_t                       size,
                        const size_t                       nmemb,
                        SerdBlockDumper* ZIX_NONNULL const dumper)
{
  SERD_DISABLE_NULL_WARNINGS

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

    // Flush block if buffer is full
    if (dumper->size == dumper->block_size) {
      dumper->out->write(
        dumper->buf, 1, dumper->block_size, dumper->out->stream);
      dumper->size = 0;
    }
  }

  SERD_RESTORE_WARNINGS
  return orig_len;
}

#endif // SERD_SRC_DUMPER_H
