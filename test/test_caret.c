// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/caret.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static int
test_caret(void)
{
  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdNode* const node  = serd_nodes_get(nodes, serd_a_string("node"));

  SerdCaret* const caret = serd_caret_new(allocator, node, 46, 2);

  assert(serd_caret_equals(caret, caret));
  assert(serd_caret_document(caret) == node);
  assert(serd_caret_line(caret) == 46);
  assert(serd_caret_column(caret) == 2);

  SerdCaret* const copy = serd_caret_copy(allocator, caret);

  assert(serd_caret_equals(caret, copy));
  assert(!serd_caret_copy(allocator, NULL));

  const SerdNode* const other_node =
    serd_nodes_get(nodes, serd_a_string("other"));

  SerdCaret* const other_file = serd_caret_new(allocator, other_node, 46, 2);
  SerdCaret* const other_line = serd_caret_new(allocator, node, 47, 2);
  SerdCaret* const other_col  = serd_caret_new(allocator, node, 46, 3);

  assert(!serd_caret_equals(caret, other_file));
  assert(!serd_caret_equals(caret, other_line));
  assert(!serd_caret_equals(caret, other_col));
  assert(!serd_caret_equals(caret, NULL));
  assert(!serd_caret_equals(NULL, caret));

  serd_caret_free(allocator, other_col);
  serd_caret_free(allocator, other_line);
  serd_caret_free(allocator, other_file);
  serd_caret_free(allocator, copy);
  serd_caret_free(allocator, caret);
  serd_nodes_free(nodes);

  return 0;
}

static void
test_failed_alloc(void)
{
  char node_buf[32];

  assert(!serd_node_construct(sizeof(node_buf), node_buf, serd_a_string("node"))
            .status);

  const SerdNode*      node      = (const SerdNode*)node_buf;
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a new caret to count the number of allocations
  SerdCaret* const caret = serd_caret_new(&allocator.base, node, 46, 2);
  assert(caret);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations;
  for (size_t i = 0U; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_caret_new(&allocator.base, node, 46, 2));
  }

  // Successfully copy the caret to count the number of allocations
  allocator.n_allocations = 0;
  allocator.n_remaining   = SIZE_MAX;
  SerdCaret* const copy   = serd_caret_copy(&allocator.base, caret);
  assert(copy);

  // Test that each allocation failing is handled gracefully
  const size_t n_copy_allocs = allocator.n_allocations;
  for (size_t i = 0U; i < n_copy_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_caret_copy(&allocator.base, caret));
  }

  serd_caret_free(&allocator.base, copy);
  serd_caret_free(&allocator.base, caret);
}

int
main(void)
{
  test_caret();
  test_failed_alloc();
  return 0;
}
