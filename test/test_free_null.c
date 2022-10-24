// Copyright 2020-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/cursor.h>
#include <serd/env.h>
#include <serd/reader.h>
#include <serd/world.h>
#include <serd/writer.h>

#include <stddef.h>

int
main(void)
{
  serd_world_free(NULL);
  serd_env_free(NULL);
  serd_reader_free(NULL);
  serd_writer_free(NULL);
  serd_cursor_free(NULL, NULL);

  return 0;
}
