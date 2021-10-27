/*
  Copyright 2021 David Robillard <d@drobilla.net>

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
#include <stdbool.h>
#include <stddef.h>

static void
test_new_failed_alloc(void)
{
  static const SerdStringView s_string = SERD_STRING("http://example.org/s");
  static const SerdStringView p_string = SERD_STRING("http://example.org/p");
  static const SerdStringView o_string = SERD_STRING("http://example.org/o");
  static const SerdStringView g_string = SERD_STRING("http://example.org/g");

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const      world = serd_world_new(&allocator.base);
  SerdNodes* const      nodes = serd_nodes_new(&allocator.base);
  const SerdNode* const s     = serd_nodes_uri(nodes, s_string);
  const SerdNode* const p     = serd_nodes_uri(nodes, p_string);
  const SerdNode* const o     = serd_nodes_uri(nodes, o_string);
  const SerdNode* const g     = serd_nodes_uri(nodes, g_string);

  SerdSink*    target         = serd_sink_new(world, NULL, NULL, NULL);
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
