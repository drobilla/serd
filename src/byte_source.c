// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"

#include "system.h"

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
    if (n_read == 0) {
      source->eof = true;
      return (source->error_func(source->stream) ? SERD_BAD_STREAM
                                                 : SERD_FAILURE);
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_open_source(SerdByteSource* const source,
                             const SerdReadFunc    read_func,
                             const SerdErrorFunc   error_func,
                             const SerdCloseFunc   close_func,
                             void* const           stream,
                             const char* const     name,
                             const size_t          page_size)
{
  const Cursor cur = {name, 1, 1};

  assert(read_func);
  assert(error_func);
  assert(page_size > 0);

  memset(source, '\0', sizeof(*source));
  source->read_func   = read_func;
  source->error_func  = error_func;
  source->close_func  = close_func;
  source->stream      = stream;
  source->page_size   = page_size;
  source->buf_size    = page_size;
  source->cur         = cur;
  source->from_stream = true;

  if (page_size > 1) {
    source->file_buf = (uint8_t*)serd_allocate_buffer(page_size);
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
serd_byte_source_open_string(SerdByteSource* const source,
                             const char* const     utf8)
{
  const Cursor cur = {"(string)", 1, 1};

  memset(source, '\0', sizeof(*source));
  source->page_size = 1;
  source->cur       = cur;
  source->read_buf  = (const uint8_t*)utf8;
  return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_close(SerdByteSource* const source)
{
  SerdStatus st = SERD_SUCCESS;
  if (source->close_func) {
    st = source->close_func(source->stream) ? SERD_BAD_STREAM : SERD_SUCCESS;
  }

  if (source->page_size > 1) {
    serd_free_aligned(source->file_buf);
  }

  memset(source, '\0', sizeof(*source));
  return st;
}
