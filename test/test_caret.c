// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/caret.h"
#include "serd/node.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static int
test_caret(void)
{
  SerdNode* const  node  = serd_new_string(NULL, zix_string("node"));
  SerdCaret* const caret = serd_caret_new(NULL, node, 46, 2);

  assert(serd_caret_equals(caret, caret));
  assert(serd_caret_document(caret) == node);
  assert(serd_caret_line(caret) == 46);
  assert(serd_caret_column(caret) == 2);

  SerdCaret* const copy = serd_caret_copy(NULL, caret);

  assert(serd_caret_equals(caret, copy));
  assert(!serd_caret_copy(NULL, NULL));

  SerdNode* const  other_node = serd_new_string(NULL, zix_string("other"));
  SerdCaret* const other_file = serd_caret_new(NULL, other_node, 46, 2);
  SerdCaret* const other_line = serd_caret_new(NULL, node, 47, 2);
  SerdCaret* const other_col  = serd_caret_new(NULL, node, 46, 3);

  assert(!serd_caret_equals(caret, other_file));
  assert(!serd_caret_equals(caret, other_line));
  assert(!serd_caret_equals(caret, other_col));
  assert(!serd_caret_equals(caret, NULL));
  assert(!serd_caret_equals(NULL, caret));

  serd_caret_free(NULL, other_col);
  serd_caret_free(NULL, other_line);
  serd_caret_free(NULL, other_file);
  serd_node_free(NULL, other_node);
  serd_caret_free(NULL, copy);
  serd_caret_free(NULL, caret);
  serd_node_free(NULL, node);

  return 0;
}

static void
test_failed_alloc(void)
{
  SerdNode* node = serd_new_token(NULL, SERD_LITERAL, zix_string("node"));

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
  serd_node_free(NULL, node);
}

int
main(void)
{
  test_caret();
  test_failed_alloc();
  return 0;
}
