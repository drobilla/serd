// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd_config.h"

#include "serd/buffer.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream.h"

// IWYU pragma: no_include <features.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

SerdOutputStream
serd_open_output_stream(SerdWriteFunc const       write_func,
                        SerdStreamCloseFunc const close_func,
                        void* const               stream)
{
  assert(write_func);

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

#ifdef __GLIBC__
  FILE* const file = fopen(path, "wbe");
#else
  FILE* const file = fopen(path, "wb");
#endif

  if (!file) {
    const SerdOutputStream failure = {NULL, NULL, NULL};
    return failure;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  (void)posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  return serd_open_output_stream(
    (SerdWriteFunc)fwrite, (SerdStreamCloseFunc)fclose, file);
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

  return ret ? SERD_ERR_UNKNOWN : SERD_SUCCESS;
}
