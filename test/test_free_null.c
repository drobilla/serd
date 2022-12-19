// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

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
  serd_model_free(NULL);
  serd_statement_free(NULL, NULL);
  serd_cursor_free(NULL, NULL);
  serd_caret_free(NULL, NULL);

  return 0;
}
