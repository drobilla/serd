// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/world.h>

#include <assert.h>
#include <stddef.h>

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a world to count the number of allocations
  SerdWorld* const world = serd_world_new(&allocator.base);
  assert(world);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_world_new(&allocator.base));
  }

  serd_world_free(world);
}

int
main(void)
{
  test_new_failed_alloc();

  return 0;
}
