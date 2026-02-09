// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"

#include "system.h"

#include <serd/input_stream.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

SerdStatus
serd_byte_source_page(SerdByteSource* const source)
{
  uint8_t* const buf =
    (source->block_size > 1 ? source->block : &source->read_byte);

  const SerdStreamResult r =
    source->in->read(source->in->stream, source->block_size, buf);

  source->buf_size  = r.count;
  source->read_head = 0;
  source->eof       = false;

  if (r.count < source->block_size) {
    buf[r.count] = '\0';
    source->eof  = !r.count;
  }

  return (r.status == SERD_NO_DATA) ? (r.count ? SERD_SUCCESS : SERD_FAILURE)
                                    : r.status;
}

static void
serd_byte_source_init_buffer(ZixAllocator* const   allocator,
                             SerdByteSource* const source)
{
  if (source->block_size > 1) {
    source->block = (uint8_t*)zix_aligned_alloc(
      allocator, SERD_PAGE_SIZE, source->block_size);

    if ((source->read_buf = source->block)) {
      memset(source->block, '\0', source->block_size);
    }
  } else {
    source->read_buf = &source->read_byte;
  }
}

SerdByteSource*
serd_byte_source_new_input(ZixAllocator* const    allocator,
                           SerdInputStream* const input,
                           const ZixStringView    name,
                           const size_t           block_size)
{
  assert(input);
  assert(block_size);
  assert(input->stream);

  char* const source_name = zix_string_view_copy(allocator, name);
  if (!source_name) {
    return NULL;
  }

  SerdByteSource* source =
    (SerdByteSource*)zix_calloc(allocator, 1, sizeof(SerdByteSource));

  if (!source) {
    zix_free(allocator, source_name);
    return NULL;
  }

  source->name                  = source_name;
  source->in                    = input;
  source->block_size            = block_size;
  source->buf_size              = 0U;
  source->caret.document.data   = source->name;
  source->caret.document.length = strlen(source->name);
  source->caret.line            = 1U;
  source->caret.column          = 1U;

  serd_byte_source_init_buffer(allocator, source);
  if (block_size > 1 && !source->block) {
    zix_free(allocator, source_name);
    zix_free(allocator, source);
    return NULL;
  }

  return source;
}

void
serd_byte_source_free(ZixAllocator* const   allocator,
                      SerdByteSource* const source)
{
  assert(source);

  if (source->block_size > 1) {
    zix_aligned_free(allocator, source->block);
  }

  zix_free(allocator, source->name);
  zix_free(allocator, source);
}

SerdStatus
serd_byte_source_prepare(SerdByteSource* const source)
{
  source->prepared = true;

  return (source->block_size > 1) ? serd_byte_source_page(source)
                                  : serd_byte_source_advance(source);
}

static SerdStatus
peek_check(SerdByteSource* const source, const uint8_t byte)
{
  return serd_byte_source_peek(source) == byte ? SERD_SUCCESS : SERD_BAD_SYNTAX;
}

SerdStatus
serd_byte_source_skip_bom(SerdByteSource* const source)
{
  SerdStatus st = SERD_SUCCESS;

  if (serd_byte_source_peek(source) == 0xEF) {
    if (!(st = serd_byte_source_advance(source)) &&
        !(st = peek_check(source, 0xBB)) &&
        !(st = serd_byte_source_advance(source)) &&
        !(st = peek_check(source, 0xBF))) {
      st = serd_byte_source_advance(source);
    } else {
      st = st > SERD_FAILURE ? st : SERD_BAD_SYNTAX;
    }
  }

  return st;
}
