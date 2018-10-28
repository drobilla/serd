// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
test_get_blank(void)
{
  SerdWorld* world = serd_world_new();
  char       expected[12];

  for (unsigned i = 0; i < 32; ++i) {
    const SerdNode* blank = serd_world_get_blank(world);

    snprintf(expected, sizeof(expected), "b%u", i + 1);
    assert(!strcmp(serd_node_string(blank), expected));
  }

  serd_world_free(world);
}

static void
test_nodes(void)
{
  SerdWorld* const world = serd_world_new();
  SerdNodes* const nodes = serd_world_nodes(world);

  assert(serd_nodes_size(nodes) > 0U);

  serd_world_free(world);
}

int
main(void)
{
  test_get_blank();
  test_nodes();

  return 0;
}
