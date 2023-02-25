// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"

#include "system.h"

#include <serd/status.h>
#include <serd/stream.h>
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
    (source->page_size > 1 ? source->file_buf : &source->read_byte);

  const size_t n_read =
    source->read_func(buf, 1, source->page_size, source->stream);

  source->buf_size  = n_read;
  source->read_head = 0;
  source->eof       = false;

  if (n_read < source->page_size) {
    buf[n_read] = '\0';
    if (!n_read) {
      source->eof = true;
      return (source->error_func(source->stream) ? SERD_BAD_STREAM
                                                 : SERD_FAILURE);
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_open_source(ZixAllocator* const   allocator,
                             SerdByteSource* const source,
                             const SerdReadFunc    read_func,
                             const SerdErrorFunc   error_func,
                             const SerdCloseFunc   close_func,
                             void* const           stream,
                             const ZixStringView   name,
                             const size_t          page_size)
{
  assert(read_func);
  assert(error_func);
  assert(page_size > 0);

  memset(source, '\0', sizeof(*source));
  source->read_func    = read_func;
  source->error_func   = error_func;
  source->close_func   = close_func;
  source->stream       = stream;
  source->page_size    = page_size;
  source->buf_size     = 0U;
  source->name         = zix_string_view_copy(allocator, name);
  source->caret.line   = 1U;
  source->caret.column = 1U;
  source->from_stream  = true;

  if (source->name) {
    source->caret.document = zix_string(source->name);
  }

  if (page_size > 1) {
    source->file_buf =
      (uint8_t*)zix_aligned_alloc(allocator, SERD_PAGE_SIZE, page_size);
    if (!source->file_buf) {
      return SERD_BAD_ALLOC;
    }

    source->read_buf = source->file_buf;
    memset(source->file_buf, '\0', page_size);
  } else {
    source->read_buf = &source->read_byte;
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_prepare(SerdByteSource* const source)
{
  if (source->page_size == 0) {
    return SERD_FAILURE;
  }

  source->prepared = true;

  if (source->from_stream) {
    return (source->page_size > 1 ? serd_byte_source_page(source)
                                  : serd_byte_source_advance(source));
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_open_string(ZixAllocator* const   allocator,
                             SerdByteSource* const source,
                             const char* const     utf8,
                             const ZixStringView   name)
{
  static const ZixStringView default_name = ZIX_STATIC_STRING("string");

  memset(source, '\0', sizeof(*source));

  source->name =
    zix_string_view_copy(allocator, name.length ? name : default_name);

  source->page_size      = 1U;
  source->read_buf       = (const uint8_t*)utf8;
  source->caret.document = zix_string(source->name);
  source->caret.line     = 1U;
  source->caret.column   = 1U;

  return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_close(ZixAllocator* const   allocator,
                       SerdByteSource* const source)
{
  SerdStatus st = SERD_SUCCESS;
  if (source->close_func) {
    st = source->close_func(source->stream) ? SERD_BAD_STREAM : SERD_SUCCESS;
  }

  if (source->page_size > 1) {
    zix_aligned_free(allocator, source->file_buf);
  }

  zix_free(allocator, source->name);
  memset(source, '\0', sizeof(*source));
  return st;
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
