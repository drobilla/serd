// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/sink.h"
#include "serd/tee.h"

#include <assert.h>
#include <stddef.h>

static void
test_failed_alloc(void)
{
  SerdSink* const target0 = serd_sink_new(NULL, NULL, NULL, NULL);
  SerdSink* const target1 = serd_sink_new(NULL, NULL, NULL, NULL);

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a sink to count the number of allocations
  SerdSink* const tee = serd_tee_new(&allocator.base, target0, target1);
  assert(tee);
  serd_sink_free(tee);

  // Test that each allocation failing is handled gracefully
  const size_t n_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_tee_new(&allocator.base, target0, target1));
  }

  serd_sink_free(target1);
  serd_sink_free(target0);
}

int
main(void)
{
  test_failed_alloc();
  return 0;
}
