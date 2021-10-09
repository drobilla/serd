// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd_config.h"

#include "serd/buffer.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream.h"

// IWYU pragma: no_include <features.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

SerdOutputStream
serd_open_output_stream(SerdWriteFunc const write_func,
                        SerdErrorFunc const error_func,
                        SerdCloseFunc const close_func,
                        void* const         stream)
{
  assert(write_func);

  SerdOutputStream output = {stream, write_func, error_func, close_func};
  return output;
}

SerdOutputStream
serd_open_output_buffer(SerdBuffer* const buffer)
{
  assert(buffer);

  return serd_open_output_stream(
    serd_buffer_write, NULL, serd_buffer_close, buffer);
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
    const SerdOutputStream failure = {NULL, NULL, NULL, NULL};
    return failure;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  (void)posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  return serd_open_output_stream(
    (SerdWriteFunc)fwrite, (SerdErrorFunc)ferror, (SerdCloseFunc)fclose, file);
}

SerdStatus
serd_close_output(SerdOutputStream* const output)
{
  if (!output || !output->stream) {
    return SERD_FAILURE;
  }

  const bool had_error = output->error ? output->error(output->stream) : false;
  int        close_st  = output->close ? output->close(output->stream) : 0;

  output->stream = NULL;

  return (had_error || close_st) ? SERD_BAD_WRITE : SERD_SUCCESS;
}
