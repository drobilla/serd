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

#ifndef SERD_BLOCK_DUMPER_H
#define SERD_BLOCK_DUMPER_H

#include "serd/serd.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  SerdAllocator* SERD_NONNULL allocator; ///< Buffer allocator

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
serd_block_dumper_open(const SerdWorld* SERD_NONNULL  world,
                       SerdBlockDumper* SERD_NONNULL  dumper,
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

#endif // SERD_DUMPER_H
