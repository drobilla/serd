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

#include "cursor.h"
#include "serd_config.h"
#include "system.h"

#include "serd/serd.h"

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
      return (source->error_func(source->stream) ? SERD_ERR_UNKNOWN
                                                 : SERD_FAILURE);
    }
  }

  return SERD_SUCCESS;
}

SerdByteSource*
serd_byte_source_new_function(const SerdReadFunc        read_func,
                              const SerdStreamErrorFunc error_func,
                              const SerdStreamCloseFunc close_func,
                              void* const               stream,
                              const SerdNode* const     name,
                              const size_t              page_size)
{
  if (!page_size) {
    return NULL;
  }

  SerdByteSource* source = (SerdByteSource*)calloc(1, sizeof(SerdByteSource));

  source->read_func  = read_func;
  source->error_func = error_func;
  source->close_func = close_func;
  source->stream     = stream;
  source->page_size  = page_size;
  source->buf_size   = page_size;
  source->type       = FROM_FUNCTION;

  source->name =
    name ? serd_node_copy(name) : serd_new_string(SERD_STATIC_STRING("func"));

  const SerdCursor cur = {source->name, 1, 1};
  source->cur          = cur;

  if (page_size > 1) {
    source->file_buf = (uint8_t*)serd_allocate_buffer(page_size);
    source->read_buf = source->file_buf;
    memset(source->file_buf, '\0', page_size);
  } else {
    source->read_buf = &source->read_byte;
  }

  return source;
}

SerdByteSource*
serd_byte_source_new_filename(const char* const path, const size_t page_size)
{
  if (page_size == 0) {
    return NULL;
  }

  FILE* const fd = fopen(path, "rb");
  if (!fd) {
    return NULL;
  }

  SerdByteSource* source = (SerdByteSource*)calloc(1, sizeof(SerdByteSource));

  source->read_func  = (SerdReadFunc)fread;
  source->error_func = (SerdStreamErrorFunc)ferror;
  source->close_func = (SerdStreamCloseFunc)fclose;
  source->stream     = fd;
  source->page_size  = page_size;
  source->buf_size   = page_size;
  source->name =
    serd_new_file_uri(SERD_MEASURE_STRING(path), SERD_EMPTY_STRING());
  source->type = FROM_FILENAME;

  const SerdCursor cur = {source->name, 1, 1};
  source->cur          = cur;

  if (page_size > 1) {
    source->file_buf = (uint8_t*)serd_allocate_buffer(page_size);
    source->read_buf = source->file_buf;
    memset(source->file_buf, '\0', page_size);
  } else {
    source->read_buf = &source->read_byte;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  posix_fadvise(fileno(fd), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  return source;
}

SerdByteSource*
serd_byte_source_new_string(const char* const     string,
                            const SerdNode* const name)
{
  SerdByteSource* source = (SerdByteSource*)calloc(1, sizeof(SerdByteSource));

  source->page_size = 1;
  source->read_buf  = (const uint8_t*)string;
  source->type      = FROM_STRING;

  source->name =
    name ? serd_node_copy(name) : serd_new_string(SERD_STATIC_STRING("string"));

  const SerdCursor cur = {source->name, 1, 1};
  source->cur          = cur;

  return source;
}

SerdStatus
serd_byte_source_prepare(SerdByteSource* const source)
{
  source->prepared = true;
  if (source->type != FROM_STRING) {
    if (source->page_size > 1) {
      return serd_byte_source_page(source);
    }

    return serd_byte_source_advance(source);
  }

  return SERD_SUCCESS;
}

void
serd_byte_source_free(SerdByteSource* const source)
{
  if (source) {
    if (source->close_func) {
      source->close_func(source->stream);
    }

    if (source->page_size > 1) {
      free(source->file_buf);
    }

    serd_node_free(source->name);
    free(source);
  }
}
