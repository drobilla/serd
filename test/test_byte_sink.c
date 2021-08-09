// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdio.h>

int
main(void)
{
  assert(!serd_byte_sink_new_filename("file.ttl", 0));
  assert(!serd_byte_sink_new_filename("/does/not/exist.ttl", 1));
  assert(!serd_byte_sink_new_function((SerdWriteFunc)fwrite, NULL, NULL, 0));

  return 0;
}
