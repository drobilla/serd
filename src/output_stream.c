// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "warnings.h"

#include "serd/buffer.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream.h"

#include <assert.h>
#include <stddef.h>

SerdOutputStream
serd_open_output_stream(SerdWriteFunc const write_func,
                        SerdCloseFunc const close_func,
                        void* const         stream)
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
