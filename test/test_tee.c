// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/sink.h>
#include <serd/tee.h>

#include <assert.h>
#include <stddef.h>

static void
test_failed_alloc(void)
{
  const SerdSink target0 = {NULL, NULL};
  const SerdSink target1 = {NULL, NULL};

  SerdFailingAllocator allocator = serd_failing_allocator();
  serd_failing_allocator_reset(&allocator, 0);

  assert(!serd_tee_new(&allocator.base, &target0, &target1));
}

int
main(void)
{
  test_failed_alloc();
  return 0;
}
