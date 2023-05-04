// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "block_dumper.h"
#include "system.h"
#include "warnings.h"

#include <stddef.h>

SerdStatus
serd_block_dumper_open(SerdBlockDumper* const  dumper,
                       SerdOutputStream* const output,
                       const size_t            block_size)
{
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

  dumper->buf = (char*)serd_allocate_buffer(block_size);
  return dumper->buf ? SERD_SUCCESS : SERD_BAD_ALLOC;
}

SerdStatus
serd_block_dumper_flush(SerdBlockDumper* const dumper)
{
  SerdStatus st = SERD_SUCCESS;

  if (dumper->out->stream && dumper->block_size > 1 && dumper->size > 0) {
    SERD_DISABLE_NULL_WARNINGS
    const size_t written =
      dumper->out->write(dumper->buf, 1, dumper->size, dumper->out->stream);
    SERD_RESTORE_WARNINGS

    st           = (written != dumper->size) ? SERD_BAD_WRITE : SERD_SUCCESS;
    dumper->size = 0;
  }

  return st;
}

void
serd_block_dumper_close(SerdBlockDumper* const dumper)
{
  serd_free_aligned(dumper->buf);
}
