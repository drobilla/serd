// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/memory.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/writer.h"

#include <stddef.h>

int
main(void)
{
  serd_free(NULL);
  serd_node_free(NULL);
  serd_env_free(NULL);
  serd_reader_free(NULL);
  serd_writer_free(NULL);

  return 0;
}
