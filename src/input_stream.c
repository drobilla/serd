// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "stream_utils.h"
#include "warnings.h"

#include "serd/input_stream.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/stream_result.h"
#include "zix/filesystem.h"

#include <assert.h>
#include <stdio.h>

static SerdStreamResult
serd_string_read(void* const stream, const size_t len, void* const buf)
{
  SerdStreamResult r = {SERD_SUCCESS, 0U};

  const char** position = (const char**)stream;

  if (len && !**position) {
    r.status = SERD_NO_DATA;
    return r;
  }

  while (r.count < len && **position) {
    ((char*)buf)[r.count++] = **position;

    ++(*position);
  }

  return r;
}

static SerdStatus
serd_string_close(void* const stream)
{
  (void)stream;
  return SERD_SUCCESS;
}

SerdInputStream
serd_open_input_stream(const SerdReadFunc  read_func,
                       const SerdCloseFunc close_func,
                       void* const         stream)
{
  assert(read_func);

  SerdInputStream input = {stream, read_func, close_func};
  return input;
}

SerdInputStream
serd_open_input_string(const char** const position)
{
  assert(position);
  assert(*position);

  const SerdInputStream input = {position, serd_string_read, serd_string_close};

  return input;
}

SerdInputStream
serd_open_input_file(const char* const path)
{
  assert(path);

  SerdInputStream input = {NULL, NULL, NULL};
  if (zix_file_type(path) == ZIX_FILE_TYPE_DIRECTORY) {
    return input;
  }

  FILE* const file = serd_fopen_wrapper(path, SERD_FILE_MODE_READ);
  if (!file) {
    return input;
  }

  return serd_open_input_stream(serd_fread_wrapper, serd_fclose_wrapper, file);
}

SerdStatus
serd_close_input(SerdInputStream* const input)
{
  SerdStatus st = SERD_SUCCESS;

  if (input) {
    if (input->close && input->stream) {
      SERD_DISABLE_NULL_WARNINGS
      st = input->close(input->stream);
      SERD_RESTORE_WARNINGS
      input->stream = NULL;
    }

    input->stream = NULL;
  }

  return st;
}
