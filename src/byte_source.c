// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"

#include "memory.h"
#include "warnings.h"

#include "serd/node.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

SerdStatus
serd_byte_source_page(SerdByteSource* const source)
{
  uint8_t* const buf =
    (source->block_size > 1 ? source->block : &source->read_byte);

  SERD_DISABLE_NULL_WARNINGS

  const size_t n_read =
    source->in->read(buf, 1, source->block_size, source->in->stream);

  source->buf_size  = n_read;
  source->read_head = 0;
  source->eof       = false;

  if (n_read < source->block_size) {
    buf[n_read] = '\0';
    if (n_read == 0) {
      source->eof = true;
      return (source->in->error(source->in->stream) ? SERD_BAD_STREAM
                                                    : SERD_FAILURE);
    }
  }

  SERD_RESTORE_WARNINGS
  return SERD_SUCCESS;
}

static SerdStatus
serd_byte_source_init_buffer(ZixAllocator* const   allocator,
                             SerdByteSource* const source)
{
  if (source->block_size > 1) {
    void* const block =
      zix_aligned_alloc(allocator, SERD_PAGE_SIZE, source->block_size);

    if (!block) {
      return SERD_BAD_ALLOC;
    }

    source->block    = (uint8_t*)block;
    source->read_buf = source->block;
    memset(source->block, '\0', source->block_size);
  } else {
    source->read_buf = &source->read_byte;
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_init(ZixAllocator* const    allocator,
                      SerdByteSource* const  source,
                      SerdInputStream* const input,
                      const SerdNode* const  name,
                      const size_t           block_size)
{
  assert(source);
  assert(input);
  assert(block_size);
  assert(input->stream);

  SerdNode* const source_name =
    name ? serd_node_copy(allocator, name)
         : serd_node_new(allocator, serd_a_string("input"));

  if (!source_name) {
    return SERD_BAD_ALLOC;
  }

  source->in             = input;
  source->read_buf       = NULL;
  source->read_head      = 0U;
  source->block_size     = (uint32_t)block_size;
  source->buf_size       = (uint32_t)block_size;
  source->caret.document = source_name;
  source->caret.line     = 1U;
  source->caret.col      = 1U;
  source->name           = source_name;
  source->block          = NULL;
  source->read_byte      = 0U;
  source->prepared       = false;

  if (serd_byte_source_init_buffer(allocator, source)) {
    serd_node_free(allocator, source_name);
    memset(source, 0, sizeof(SerdByteSource));
    return SERD_BAD_ALLOC;
  }

  return SERD_SUCCESS;
}

void
serd_byte_source_destroy(ZixAllocator* const   allocator,
                         SerdByteSource* const source)
{
  if (source->block_size > 1) {
    zix_aligned_free(allocator, source->block);
  }

  serd_node_free(allocator, source->name);
  memset(source, 0, sizeof(SerdByteSource));
}

SerdStatus
serd_byte_source_prepare(SerdByteSource* const source)
{
  source->prepared = true;
  return serd_byte_source_page(source);
}

SerdStatus
serd_byte_source_skip_bom(SerdByteSource* const source)
{
  if (serd_byte_source_peek(source) == 0xEF) {
    if (serd_byte_source_advance_past(source, 0xEF) ||
        serd_byte_source_peek(source) != 0xBB ||
        serd_byte_source_advance_past(source, 0xBB) ||
        serd_byte_source_peek(source) != 0xBF ||
        serd_byte_source_advance_past(source, 0xBF)) {
      return SERD_BAD_SYNTAX;
    }
  }

  return SERD_SUCCESS;
}
