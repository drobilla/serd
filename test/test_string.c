// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/status.h>
#include <serd/string.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <string.h>

static void
test_expect_string(void)
{
  assert(expect_string("match", "match"));
  assert(!expect_string("intentional", "failure"));
}

static void
test_expect_string_view(void)
{
  assert(expect_string_view(zix_empty_string(), ""));
  assert(expect_string_view(zix_string("match"), "match"));
  assert(!expect_string_view(zix_string("intentional"), "failure"));
}

static void
test_new(void)
{
  const SerdString empty = serd_string_new(NULL, zix_empty_string());
  assert(!empty.length);
  assert(!empty.data);

  const SerdString hello = serd_string_new(NULL, zix_string("hello"));
  assert(zix_string_view_equals(serd_string_view(hello), zix_string("hello")));
  zix_free(NULL, hello.data);
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a canon to count the number of allocations
  const SerdString hello = serd_string_new(NULL, zix_string("hello"));
  assert(zix_string_view_equals(serd_string_view(hello), zix_string("hello")));
  zix_free(NULL, hello.data);

  serd_failing_allocator_reset(&allocator, 0);

  // Test that failed allocation is handled gracefully
  const SerdString failed =
    serd_string_new(&allocator.base, zix_string("hello"));
  assert(!failed.length);
  assert(!failed.data);
  assert(!serd_string_view(failed).length);
  assert(serd_string_view(failed).data[0] == '\0');
}

static void
test_strerror(void)
{
  const char* msg = serd_strerror(SERD_SUCCESS);
  assert(expect_string(msg, "Success"));
  for (int i = SERD_FAILURE; i <= SERD_BAD_PATTERN; ++i) {
    msg = serd_strerror((SerdStatus)i);
    assert(strcmp(msg, "Success"));
  }

  msg = serd_strerror((SerdStatus)-1);
  assert(expect_string(msg, "Unknown error"));
}

ZIX_PURE_FUNC int
main(void)
{
  test_expect_string();
  test_expect_string_view();
  test_new();
  test_new_failed_alloc();
  test_strerror();
  return 0;
}
