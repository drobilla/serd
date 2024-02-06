// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "stream_utils.h"
#include "serd_config.h"

#include "serd/status.h"
#include "serd/stream.h"
#include "serd/stream_result.h"

#include <assert.h>
#include <stdio.h>

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

// IWYU pragma: no_include <features.h>

static void
serd_fadvise_sequential(FILE* const file)
{
#if USE_POSIX_FADVISE && USE_FILENO
  (void)posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#else
  (void)stream;
#endif
}

FILE*
serd_fopen_wrapper(const char* path, const SerdFileMode mode)
{
#ifdef __GLIBC__
#  define MODE_SUFFIX "e"
#else
#  define MODE_SUFFIX ""
#endif

  const char* const fopen_mode =
    (mode == SERD_FILE_MODE_READ) ? ("rb" MODE_SUFFIX) : ("wb" MODE_SUFFIX);

  FILE* const file = fopen(path, fopen_mode);
  if (file) {
    serd_fadvise_sequential(file);
  }

  return file;

#undef MODE_SUFFIX
}

SerdStatus
serd_fclose_wrapper(void* const stream)
{
  FILE* const file = (FILE*)stream;

  return fclose(file) ? SERD_BAD_STREAM : SERD_SUCCESS;
}

SerdStreamResult
serd_fread_wrapper(void* const stream, const size_t len, void* const buf)
{
  SerdStreamResult r    = {SERD_SUCCESS, 0U};
  FILE* const      file = (FILE*)stream;

  r.count = fread(buf, 1U, len, file);

  if (r.count != len) {
    if (ferror(file)) {
      r.status = SERD_BAD_READ;
    } else if (feof(file)) {
      r.status = SERD_NO_DATA;
    }
  }

  return r;
}

SerdStreamResult
serd_fwrite_wrapper(void* const stream, const size_t len, const void* const buf)
{
  FILE* const file = (FILE*)stream;

  if (!len) {
    const SerdStreamResult r = {SERD_SUCCESS, 0U};
    return r;
  }

  assert(len > 0);

  const size_t           n = fwrite(buf, 1U, len, file);
  const SerdStreamResult r = {n == len ? SERD_SUCCESS : SERD_BAD_WRITE, n};
  return r;
}
