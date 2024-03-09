// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/caret.h"
#include "serd/cursor.h"
#include "serd/env.h"
#include "serd/model.h"
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
  serd_caret_free(NULL, NULL);
  serd_model_free(NULL);
  serd_cursor_free(NULL, NULL);

  return 0;
}
