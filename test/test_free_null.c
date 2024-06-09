// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/world.h"
#include "serd/writer.h"

#include <stddef.h>

int
main(void)
{
  serd_node_free(NULL, NULL);
  serd_world_free(NULL);
  serd_env_free(NULL);
  serd_sink_free(NULL);
  serd_reader_free(NULL);
  serd_writer_free(NULL);
  serd_nodes_free(NULL);

  return 0;
}
