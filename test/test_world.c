/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocatorState state     = {0u, SIZE_MAX};
  SerdAllocator             allocator = serd_failing_allocator(&state);

  // Successfully allocate a world to count the number of allocations
  SerdWorld* const world = serd_world_new(&allocator);
  assert(world);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = state.n_allocations;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    state.n_remaining = i;
    assert(!serd_world_new(&allocator));
  }

  serd_world_free(world);
}

static void
test_get_blank(void)
{
  SerdWorld* world = serd_world_new(NULL);
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
  SerdWorld* const world = serd_world_new(NULL);
  SerdNodes* const nodes = serd_world_nodes(world);

  assert(serd_nodes_size(nodes) > 0u);

  serd_world_free(world);
}

int
main(void)
{
  test_new_failed_alloc();
  test_get_blank();
  test_nodes();

  return 0;
}
