// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/canon.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stddef.h>

static SerdStatus
ignore_event(void* handle, const SerdEvent* event)
{
  (void)handle;
  (void)event;
  return SERD_SUCCESS;
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  SerdNodes* const nodes = serd_nodes_new(&allocator.base);

  SerdSink* target = serd_sink_new(&allocator.base, NULL, ignore_event, NULL);
  const size_t n_setup_allocs = allocator.n_allocations;

  // Successfully allocate a canon to count the number of allocations
  SerdSink* canon = serd_canon_new(world, target, 0U);
  assert(canon);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_canon_new(world, target, 0U));
  }

  serd_sink_free(canon);
  serd_sink_free(target);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

static void
test_write_failed_alloc(void)
{
#define NS_EG "http://example.org/s"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

  static const ZixStringView s_string = ZIX_STATIC_STRING(NS_EG "s");
  static const ZixStringView p_string = ZIX_STATIC_STRING(NS_EG "p");
  static const ZixStringView o_string = ZIX_STATIC_STRING("012.340");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdWorld* const     world     = serd_world_new(&allocator.base);
  SerdNodes* const     nodes     = serd_nodes_new(&allocator.base);

  const SerdNode* const xsd_float =
    serd_nodes_get(nodes, serd_a_uri_string(NS_XSD "float"));

  const SerdNode* const s = serd_nodes_get(nodes, serd_a_uri(s_string));
  const SerdNode* const p = serd_nodes_get(nodes, serd_a_uri(p_string));
  const SerdNode* const o =
    serd_nodes_get(nodes, serd_a_typed_literal(o_string, xsd_float));

  SerdSink* target = serd_sink_new(&allocator.base, NULL, ignore_event, NULL);
  SerdSink* canon  = serd_canon_new(world, target, 0U);

  const size_t n_setup_allocs = allocator.n_allocations;

  // Successfully write statement to count the number of allocations
  assert(canon);
  assert(!serd_sink_write(canon, 0U, s, p, o, NULL));

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;

    const SerdStatus st = serd_sink_write(canon, 0U, s, p, o, NULL);
    assert(st == SERD_BAD_ALLOC);
  }

  serd_sink_free(canon);
  serd_sink_free(target);
  serd_nodes_free(nodes);
  serd_world_free(world);

#undef NS_XSD
#undef NS_EG
}

int
main(void)
{
  test_new_failed_alloc();
  test_write_failed_alloc();
  return 0;
}
