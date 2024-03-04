// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/buffer.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

size_t
serd_buffer_write(const void* const buf,
                  const size_t      size,
                  const size_t      nmemb,
                  void* const       stream)
{
  assert(buf);
  assert(stream);

  SerdBuffer* const buffer  = (SerdBuffer*)stream;
  const size_t      n_bytes = size * nmemb;

  char* const new_buf =
    (char*)zix_realloc(buffer->allocator, buffer->buf, buffer->len + n_bytes);

  if (new_buf) {
    memcpy(new_buf + buffer->len, buf, n_bytes);
    buffer->buf = new_buf;
    buffer->len += nmemb;
    return n_bytes;
  }

  return 0;
}

int
serd_buffer_close(void* const stream)
{
  return serd_buffer_write("", 1, 1, stream) != 1; // Write null terminator
}
