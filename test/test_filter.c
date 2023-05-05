// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/filter.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/sink.h"
#include "serd/world.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static void
test_new_failed_alloc(void)
{
  const SerdNodeArgs s_args = serd_a_uri_string("http://example.org/s");
  const SerdNodeArgs p_args = serd_a_uri_string("http://example.org/p");
  const SerdNodeArgs o_args = serd_a_uri_string("http://example.org/o");
  const SerdNodeArgs g_args = serd_a_uri_string("http://example.org/g");

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  SerdNodes* const nodes = serd_nodes_new(&allocator.base);

  const SerdNode* const s = serd_nodes_get(nodes, s_args);
  const SerdNode* const p = serd_nodes_get(nodes, p_args);
  const SerdNode* const o = serd_nodes_get(nodes, o_args);
  const SerdNode* const g = serd_nodes_get(nodes, g_args);

  SerdSink*    target = serd_sink_new(&allocator.base, NULL, NULL, NULL);
  const size_t n_setup_allocs = allocator.n_allocations;

  // Successfully allocate a filter to count the number of allocations
  SerdSink* filter = serd_filter_new(world, target, s, p, o, g, true);
  assert(filter);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_filter_new(world, target, s, p, o, g, true));
  }

  serd_sink_free(filter);
  serd_sink_free(target);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

int
main(void)
{
  test_new_failed_alloc();
  return 0;
}
