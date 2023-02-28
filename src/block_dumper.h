// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BLOCK_DUMPER_H
#define SERD_SRC_BLOCK_DUMPER_H

#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>
#include <string.h>

/// The partially inlinable sink interface used by the writer
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

   This is typically used before closing to ensure that all of the writes have
   actually been written to disk.
*/
SerdStatus
serd_block_dumper_flush(SerdBlockDumper* ZIX_NONNULL dumper);

void
serd_block_dumper_close(SerdBlockDumper* ZIX_NONNULL dumper);

/**
   Write some bytes to the page writer.

   This works like any other SerdWriteFunc, but will append to an internal
   buffer and only actually write to the output when a whole block is ready.
*/
static inline SerdWriteResult
serd_block_dumper_write(SerdBlockDumper* ZIX_NONNULL const dumper,
                        const void* ZIX_NONNULL            buf,
                        const size_t                       size)
{
  SerdWriteResult               result     = {SERD_SUCCESS, 0U};
  const size_t                  block_size = dumper->block_size;
  const SerdOutputStream* const out        = dumper->out;

  if (block_size == 1) {
    result.count  = out->write(buf, 1U, size, out->stream);
    result.status = result.count == size ? SERD_SUCCESS : SERD_BAD_WRITE;
    return result;
  }

  while (result.count < size) {
    const size_t unwritten = size - result.count;
    const size_t space     = block_size - dumper->size;
    const size_t n         = space < unwritten ? space : unwritten;

    // Write as much as possible into the remaining buffer space
    memcpy(dumper->buf + dumper->size, (const char*)buf + result.count, n);
    dumper->size += n;
    result.count += n;

    // Flush page if buffer is full
    if (dumper->size == block_size) {
      if (out->write(dumper->buf, 1, block_size, out->stream) != block_size) {
        result.status = SERD_BAD_WRITE;
        return result;
      }

      dumper->size = 0;
    }
  }

  return result;
}

#endif // SERD_SRC_DUMPER_H
