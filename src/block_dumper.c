// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "block_dumper.h"

#include "serd_config.h"

#include <serd/output_stream.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <serd/world.h>
#include <zix/allocator.h>

SerdStatus
serd_block_dumper_open(const SerdWorld* const        world,
                       SerdBlockDumper* const        dumper,
                       const SerdOutputStream* const output,
                       const size_t                  block_size)
{
  ZixAllocator* const allocator = serd_world_allocator(world);

  if (!block_size) {
    return SERD_BAD_ARG;
  }

  dumper->out        = output;
  dumper->buf        = NULL;
  dumper->size       = 0U;
  dumper->block_size = block_size;

  if (block_size == 1) {
    return SERD_SUCCESS;
  }

  dumper->buf = (char*)zix_aligned_alloc(allocator, SERD_PAGE_SIZE, block_size);
  return dumper->buf ? SERD_SUCCESS : SERD_BAD_ALLOC;
}

SerdStatus
serd_block_dumper_flush(SerdBlockDumper* const dumper)
{
  SerdStatus st = SERD_SUCCESS;

  if (dumper->out->stream && dumper->block_size > 1 && dumper->size > 0) {
    const SerdStreamResult r =
      dumper->out->write(dumper->out->stream, dumper->size, dumper->buf);

    st           = r.status;
    dumper->size = 0;
  }

  return st;
}

void
serd_block_dumper_close(SerdBlockDumper* const dumper)
{
  zix_aligned_free(dumper->allocator, dumper->buf);
}
