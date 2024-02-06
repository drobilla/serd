// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/buffer.h"
#include "serd/status.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

SerdStreamResult
serd_buffer_write(const void* const buf, const size_t len, void* const stream)
{
  assert(buf);
  assert(stream);

  SerdStreamResult  r      = {SERD_SUCCESS, 0U};
  SerdBuffer* const buffer = (SerdBuffer*)stream;

  char* const new_buf =
    (char*)zix_realloc(buffer->allocator, buffer->buf, buffer->len + len);

  if (new_buf) {
    memcpy(new_buf + buffer->len, buf, len);
    buffer->buf = new_buf;
    buffer->len += len;
    r.count = len;
  } else {
    r.status = SERD_BAD_ALLOC;
  }

  return r;
}

SerdStatus
serd_buffer_close(void* const stream)
{
  return serd_buffer_write("", 1, stream).status; // Write null terminator
}
