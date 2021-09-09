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

#define _POSIX_C_SOURCE 200809L /* for posix_fadvise and fileno */

#include "serd_config.h"

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

SerdOutputStream
serd_open_output_stream(SerdWriteFunc const write_func,
                        SerdCloseFunc const close_func,
                        void* const         stream)
{
  SerdOutputStream output = {stream, write_func, close_func};
  return output;
}

SerdOutputStream
serd_open_output_buffer(SerdBuffer* const buffer)
{
  assert(buffer);

  return serd_open_output_stream(serd_buffer_write, serd_buffer_close, buffer);
}

SerdOutputStream
serd_open_output_file(const char* const path)
{
  assert(path);

  FILE* const file = fopen(path, "wb");
  if (!file) {
    const SerdOutputStream failure = {NULL, NULL, NULL};
    return failure;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  return serd_open_output_stream(
    (SerdWriteFunc)fwrite, (SerdCloseFunc)fclose, file);
}

SerdStatus
serd_close_output(SerdOutputStream* const output)
{
  int ret = 0;

  if (output) {
    if (output->close && output->stream) {
      ret            = output->close(output->stream);
      output->stream = NULL;
    }

    output->stream = NULL;
  }

  return ret ? SERD_BAD_WRITE : SERD_SUCCESS;
}
