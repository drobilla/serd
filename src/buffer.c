// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/buffer.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
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

  char* const new_buf = (char*)realloc(buffer->buf, buffer->len + n_bytes);
  if (new_buf) {
    memcpy(new_buf + buffer->len, buf, n_bytes);
    buffer->buf = new_buf;
    buffer->len += nmemb;
  }

  return new_buf ? nmemb : 0;
}

int
serd_buffer_close(void* const stream)
{
  serd_buffer_write("", 1, 1, stream); // Write null terminator

  return 0;
}
