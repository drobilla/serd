// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BLOCK_DUMPER_H
#define SERD_SRC_BLOCK_DUMPER_H

#include "warnings.h"

#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  ZixAllocator* ZIX_NONNULL       allocator;  ///< Buffer allocator
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
serd_block_dumper_open(const SerdWorld* ZIX_NONNULL  world,
                       SerdBlockDumper* ZIX_NONNULL  dumper,
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
static inline SerdStreamResult
serd_block_dumper_write(const void* ZIX_NONNULL      buf,
                        const size_t                 nmemb,
                        SerdBlockDumper* ZIX_NONNULL dumper)
{
  SerdStreamResult              result     = {SERD_SUCCESS, 0U};
  const size_t                  block_size = dumper->block_size;
  const SerdOutputStream* const out        = dumper->out;

  SERD_DISABLE_NULL_WARNINGS

  if (block_size == 1) {
    return out->write(out->stream, nmemb, buf);
  }

  while (!result.status && result.count < nmemb) {
    const size_t unwritten = nmemb - result.count;
    const size_t space     = block_size - dumper->size;
    const size_t n         = space < unwritten ? space : unwritten;

    // Write as much as possible into the remaining buffer space
    memcpy(dumper->buf + dumper->size, (const char*)buf + result.count, n);
    dumper->size += n;
    result.count += n;

    // Flush block if buffer is full
    if (dumper->size == block_size) {
      result.status = out->write(out->stream, block_size, dumper->buf).status;
      dumper->size  = 0;
    }
  }

  SERD_RESTORE_WARNINGS
  return result;
}

#endif // SERD_SRC_DUMPER_H
