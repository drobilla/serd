// Copyright 2021-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/env.h>
#include <serd/filter.h>
#include <serd/handler.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static void
test_new_failed_alloc(void)
{
  const SerdTokenView  g = {SERD_URI, zix_string("http://example.org/g")};
  const SerdTokenView  s = {SERD_URI, zix_string("http://example.org/s")};
  const SerdTokenView  p = {SERD_URI, zix_string("http://example.org/p")};
  const SerdObjectView o = {
    SERD_URI, zix_string("http://example.org/o"), 0U, {SERD_LITERAL, {"", 0U}}};

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  assert(world);

  SerdEnv* const env = serd_env_new(&allocator.base, zix_empty_string());

  const SerdSink target = {NULL, NULL};
  serd_failing_allocator_reset(&allocator, SIZE_MAX);

  // Successfully allocate a filter to count the number of allocations
  SerdHandler* const filter =
    serd_filter_new(world, env, &target, s, p, o, g, true);
  assert(filter);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_filter_new(world, env, &target, s, p, o, g, true));
  }

  serd_handler_free(filter);
  serd_env_free(env);
  serd_world_free(world);
}

int
main(void)
{
  test_new_failed_alloc();
  return 0;
}
