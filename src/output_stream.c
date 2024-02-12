// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "warnings.h"

#include "serd/buffer.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/stream_result.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

SerdOutputStream
serd_open_output_stream(SerdWriteFunc const write_func,
                        SerdCloseFunc const close_func,
                        void* const         stream)
{
  assert(write_func);

  SerdOutputStream output = {stream, write_func, close_func};
  return output;
}

static SerdStreamResult
serd_buffer_write(void* const stream, const size_t len, const void* const buf)
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

static SerdStatus
serd_buffer_close(void* const stream)
{
  return serd_buffer_write(stream, 1, "").status; // Write null terminator
}

SerdOutputStream
serd_open_output_buffer(SerdBuffer* const buffer)
{
  assert(buffer);

  return serd_open_output_stream(serd_buffer_write, serd_buffer_close, buffer);
}

SerdStatus
serd_close_output(SerdOutputStream* const output)
{
  if (!output || !output->stream) {
    return SERD_FAILURE;
  }

  SERD_DISABLE_NULL_WARNINGS
  const SerdStatus st =
    output->close ? output->close(output->stream) : SERD_SUCCESS;
  SERD_RESTORE_WARNINGS

  output->stream = NULL;

  return st;
}
