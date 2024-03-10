// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd_config.h"
#include "warnings.h"

#include "serd/input_stream.h"
#include "serd/status.h"
#include "serd/stream.h"

#include <sys/stat.h>

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

// IWYU pragma: no_include <features.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static size_t
serd_string_read(void* const  buf,
                 const size_t size,
                 const size_t nmemb,
                 void* const  stream)
{
  const char** position = (const char**)stream;

  size_t       n_read = 0U;
  const size_t len    = size * nmemb;
  while (n_read < len && **position) {
    ((char*)buf)[n_read++] = **position;

    ++(*position);
  }

  return n_read;
}

static int
serd_string_error(void* const stream)
{
  (void)stream;
  return 0;
}

static int
serd_string_close(void* const stream)
{
  (void)stream;
  return 0;
}

SerdInputStream
serd_open_input_stream(const SerdReadFunc  read_func,
                       const SerdErrorFunc error_func,
                       const SerdCloseFunc close_func,
                       void* const         stream)
{
  assert(read_func);
  assert(error_func);

  SerdInputStream input = {stream, read_func, error_func, close_func};
  return input;
}

static bool
is_directory(const char* const path)
{
#ifdef _MSC_VER
  struct stat st;
  return !stat(path, &st) && (st.st_mode & _S_IFDIR);
#else
  struct stat st;
  return !stat(path, &st) && S_ISDIR(st.st_mode);
#endif
}

SerdInputStream
serd_open_input_string(const char** const position)
{
  assert(position);
  assert(*position);

  const SerdInputStream input = {
    position, serd_string_read, serd_string_error, serd_string_close};

  return input;
}

SerdInputStream
serd_open_input_file(const char* const path)
{
  assert(path);

  SerdInputStream input = {NULL, NULL, NULL, NULL};
  if (is_directory(path)) {
    return input;
  }

#ifdef __GLIBC__
  FILE* const file = fopen(path, "rbe");
#else
  FILE* const file = fopen(path, "rb");
#endif

  if (!file) {
    return input;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  (void)posix_fadvise(fileno(file), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  input.stream = file;
  input.read   = (SerdReadFunc)fread;
  input.error  = (SerdErrorFunc)ferror;
  input.close  = (SerdCloseFunc)fclose;

  return input;
}

SerdStatus
serd_close_input(SerdInputStream* const input)
{
  int ret = 0;

  if (input) {
    if (input->close && input->stream) {
      SERD_DISABLE_NULL_WARNINGS
      ret = input->close(input->stream);
      SERD_RESTORE_WARNINGS
      input->stream = NULL;
    }

    input->stream = NULL;
  }

  return ret ? SERD_BAD_STREAM : SERD_SUCCESS;
}
