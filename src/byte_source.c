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

#include "byte_source.h"

#include "caret.h"
#include "system.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

SerdStatus
serd_byte_source_page(SerdByteSource* const source)
{
  uint8_t* const buf =
    (source->block_size > 1 ? source->block : &source->read_byte);

  const size_t n_read =
    source->in->read(buf, 1, source->block_size, source->in->stream);

  source->buf_size  = n_read;
  source->read_head = 0;
  source->eof       = false;

  if (n_read < source->block_size) {
    buf[n_read] = '\0';
    if (n_read == 0) {
      source->eof = true;
      return (source->in->error(source->in->stream) ? SERD_ERR_UNKNOWN
                                                    : SERD_FAILURE);
    }
  }

  return SERD_SUCCESS;
}

static void
serd_byte_source_init_buffer(SerdByteSource* const source)
{
  if (source->block_size > 1) {
    source->block    = (uint8_t*)serd_allocate_buffer(source->block_size);
    source->read_buf = source->block;
    memset(source->block, '\0', source->block_size);
  } else {
    source->read_buf = &source->read_byte;
  }
}

SerdByteSource*
serd_byte_source_new_input(SerdInputStream* const input,
                           const SerdNode* const  name,
                           const size_t           block_size)
{
  assert(input);

  if (!block_size || !input->stream) {
    return NULL;
  }

  SerdByteSource* source = (SerdByteSource*)calloc(1, sizeof(SerdByteSource));

  source->name =
    name ? serd_node_copy(name) : serd_new_string(SERD_STRING("input"));

  source->in         = input;
  source->block_size = block_size;
  source->buf_size   = block_size;
  source->caret.file = source->name;
  source->caret.line = 1u;
  source->caret.col  = 1u;

  serd_byte_source_init_buffer(source);

  return source;
}

void
serd_byte_source_free(SerdByteSource* const source)
{
  if (source) {
    serd_close_input(source->in);

    if (source->block_size > 1) {
      serd_free_aligned(source->block);
    }

    serd_node_free(source->name);
    free(source);
  }
}

SerdStatus
serd_byte_source_prepare(SerdByteSource* const source)
{
  source->prepared = true;

  if (source->block_size > 1) {
    return serd_byte_source_page(source);
  }

  return serd_byte_source_advance(source);
}
