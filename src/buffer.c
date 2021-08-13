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

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
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
