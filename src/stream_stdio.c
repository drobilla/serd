// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd_config.h"

#include "serd/input_stream.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "zix/filesystem.h"

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

#ifdef _WIN32
#  ifdef _MSC_VER
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include <fcntl.h>
#  include <io.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

// IWYU pragma: no_include <features.h>

/* Common */

typedef enum {
  SERD_FILE_MODE_READ,
  SERD_FILE_MODE_WRITE,
} SerdFileMode;

static void*
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
  if (!file) {
    return NULL;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  (void)posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  return file;

#undef MODE_SUFFIX
}

static void
serd_set_stream_utf8_mode(FILE* const stream)
{
#ifdef _WIN32
  _setmode(_fileno(stream), _O_BINARY);
#else
  (void)stream;
#endif
}

static SerdStatus
serd_fclose_wrapper(void* const stream)
{
  FILE* const file = (FILE*)stream;

  return fclose(file) ? SERD_BAD_STREAM : SERD_SUCCESS;
}

/* Input */

static SerdStreamResult
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

SerdInputStream
serd_open_input_file(const char* const path)
{
  assert(path);

  SerdInputStream input = {NULL, NULL, NULL};
  if (zix_file_type(path) == ZIX_FILE_TYPE_DIRECTORY) {
    return input;
  }

  void* const file = serd_fopen_wrapper(path, SERD_FILE_MODE_READ);
  if (!file) {
    return input;
  }

  return serd_open_input_stream(serd_fread_wrapper, serd_fclose_wrapper, file);
}

/// Faster fread-compatible wrapper for reading single bytes
static SerdStreamResult
serd_file_read_byte(void* const stream, const size_t len, void* const buf)
{
  assert(len == 1U);
  (void)len;

  SerdStreamResult r = {SERD_SUCCESS, 0U};

  const int c = getc((FILE*)stream);
  if (c == EOF) {
    *((uint8_t*)buf) = 0;
    r.status         = SERD_NO_DATA;
  } else {
    *((uint8_t*)buf) = (uint8_t)c;
    r.count          = 1U;
  }

  return r;
}

SerdInputStream
serd_open_input_standard(void)
{
  serd_set_stream_utf8_mode(stdin);
  return serd_open_input_stream(
    serd_file_read_byte, serd_fclose_wrapper, stdin);
}

/* Output */

static SerdStreamResult
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

SerdOutputStream
serd_open_output_file(const char* const path)
{
  assert(path);

  const SerdOutputStream output = {NULL, NULL, NULL};
  if (zix_file_type(path) == ZIX_FILE_TYPE_DIRECTORY) {
    errno = EISDIR;
    return output;
  }

  void* const file = serd_fopen_wrapper(path, SERD_FILE_MODE_WRITE);
  if (!file) {
    return output;
  }

  return serd_open_output_stream(
    serd_fwrite_wrapper, serd_fclose_wrapper, file);
}

SerdOutputStream
serd_open_output_standard(void)
{
  serd_set_stream_utf8_mode(stdout);
  return serd_open_output_stream(
    serd_fwrite_wrapper, serd_fclose_wrapper, stdout);
}
