/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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

#include "block_dumper.h"
#include "system.h"

#include "serd/serd.h"

#include <stddef.h>

SerdStatus
serd_block_dumper_open(SerdBlockDumper* const  dumper,
                       SerdOutputStream* const output,
                       const size_t            block_size)
{
  if (!block_size) {
    return SERD_ERR_BAD_ARG;
  }

  dumper->out        = output;
  dumper->buf        = NULL;
  dumper->size       = 0u;
  dumper->block_size = block_size;

  if (block_size == 1) {
    return SERD_SUCCESS;
  }

  dumper->buf = (char*)serd_allocate_buffer(block_size);
  return dumper->buf ? SERD_SUCCESS : SERD_ERR_INTERNAL;
}

void
serd_block_dumper_flush(SerdBlockDumper* const dumper)
{
  if (dumper->out->stream && dumper->block_size > 1 && dumper->size > 0) {
    dumper->out->write(dumper->buf, 1, dumper->size, dumper->out->stream);
    dumper->size = 0;
  }
}

void
serd_block_dumper_close(SerdBlockDumper* const dumper)
{
  serd_free_aligned(dumper->buf);
}
