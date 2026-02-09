// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/input_stream.h>
#include <serd/status.h>
#include <serd/stream.h>
#include <serd/stream_result.h>

#include <assert.h>
#include <stddef.h>

static SerdStreamResult
serd_string_read(void* const stream, const size_t len, void* const buf)
{
  SerdStreamResult r = {SERD_SUCCESS, 0U};

  const char** const p = (const char**)stream;

  for (; r.count < len && (*p)[r.count]; ++r.count) {
    ((char*)buf)[r.count] = (*p)[r.count];
  }

  if (r.count != len) {
    r.status = SERD_NO_DATA;
  }

  *p += r.count;
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

SerdStatus
serd_close_input(SerdInputStream* const input)
{
  assert(input);

  const SerdStatus st =
    input->close ? input->close(input->stream) : SERD_SUCCESS;

  input->stream = NULL;
  return st;
}
