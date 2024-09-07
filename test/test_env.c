// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/env.h>
#include <serd/status.h>
#include <serd/uri.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define NS_EG "http://example.org/"

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a env to count the number of allocations
  SerdEnv* const env = serd_env_new(&allocator.base, zix_empty_string());
  assert(env);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_env_new(&allocator.base, zix_empty_string()));
  }

  serd_env_free(env);
}

static void
test_set_base_failed_alloc(void)
{
  static const ZixStringView base_uri =
    ZIX_STATIC_STRING("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env = serd_env_new(&allocator.base, zix_empty_string());

  serd_failing_allocator_reset(&allocator, 0);

  const SerdStatus st = serd_env_set_base_uri(env, base_uri);
  assert(st == SERD_BAD_ALLOC);

  serd_env_free(env);
}

static void
test_set_prefix_existing_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env = serd_env_new(&allocator.base, zix_empty_string());
  SerdStatus           st  = SERD_SUCCESS;

  serd_failing_allocator_reset(&allocator, 3);

  st = serd_env_set_prefix(
    env, zix_string("eg"), zix_string("http://example.com/"));
  assert(st == SERD_SUCCESS);

  st = serd_env_set_prefix(
    env, zix_string("eg"), zix_string("http://example.org/"));
  assert(st == SERD_BAD_ALLOC);

  serd_env_free(env);
}

static void
test_set_prefix_absolute_failed_alloc(void)
{
  static const ZixStringView base_uri =
    ZIX_STATIC_STRING("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env       = serd_env_new(&allocator.base, base_uri);
  SerdStatus           st        = SERD_SUCCESS;
  char                 name[64]  = "eg";
  char                 uri[64]   = "http://example.org/";

  // Successfully set an absolute prefix to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
  assert(st == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_prefix_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_prefix_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);

    snprintf(name, sizeof(name), "eg%zu", i);
    snprintf(uri, sizeof(name), "http://example.org/%zu", i);

    st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
    assert(st == SERD_BAD_ALLOC);
  }

  serd_env_free(env);
}

static void
test_set_prefix_relative_failed_alloc(void)
{
  static const ZixStringView base_uri =
    ZIX_STATIC_STRING("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env       = serd_env_new(&allocator.base, base_uri);
  SerdStatus           st        = SERD_SUCCESS;
  char                 name[64]  = "egX";
  char                 uri[64]   = "relativeX";

  // Successfully set an absolute prefix to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
  assert(st == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_prefix_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_prefix_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);

    snprintf(name, sizeof(name), "eg%zu", i);
    snprintf(uri, sizeof(uri), "relative%zu", i);

    st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
    assert(st == SERD_BAD_ALLOC);
  }

  serd_env_free(env);
}

static void
test_null(void)
{
  // Accessors are tolerant to a NULL env for convenience
  ZixStringView prefix = {NULL, 0U};
  ZixStringView suffix = {NULL, 0U};
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(NULL)));
  assert(!serd_env_prefix_uri(NULL, zix_string("name")).length);
  assert(serd_env_expand(NULL, zix_empty_string(), &prefix, &suffix) ==
         SERD_BAD_ARG);
  assert(serd_env_qualify(NULL, zix_empty_string(), &prefix, &suffix) ==
         SERD_BAD_ARG);
}

static void
test_base_uri(void)
{
  assert(!serd_env_new(NULL, zix_string("rel")));
  assert(!serd_env_base_uri_string(NULL).length);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  // Test that empty/unset base works as expected
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(env)));
  assert(!serd_env_base_uri_string(env).length);
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(env)));
  assert(!serd_env_base_uri_string(env).length);

  // Try setting a relative base with no previous base URI
  assert(serd_env_set_base_uri(env, zix_string("rel")) == SERD_BAD_ARG);

  // Try setting a relative prefix with no base URI
  assert(serd_env_set_prefix(env, zix_string("eg.3"), zix_string("rel")) ==
         SERD_BAD_ARG);

  // Set a valid base URI
  assert(!serd_env_set_base_uri(env, zix_string(NS_EG)));
  assert(expect_string_view(serd_env_base_uri_string(env), NS_EG));

  // Reset the base URI
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(expect_string_view(serd_env_base_uri_string(env), ""));
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(env)));

  serd_env_free(env);
}

static void
test_set_prefix(void)
{
  static const ZixStringView eg    = ZIX_STATIC_STRING(NS_EG);
  static const ZixStringView name1 = ZIX_STATIC_STRING("eg.1");
  static const ZixStringView name2 = ZIX_STATIC_STRING("eg.2");
  static const ZixStringView rel   = ZIX_STATIC_STRING("rel");
  static const ZixStringView base  = ZIX_STATIC_STRING(NS_EG);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  // Ensure that a prefix isn't initially set
  assert(!serd_env_prefix_uri(env, name1).length);
  assert(serd_env_prefix_uri(env, name1).data);
  assert(serd_env_prefix_uri(env, name1).data[0] == '\0');

  // Set a valid prefix
  assert(!serd_env_set_prefix(env, name1, eg));
  assert(expect_string_view(serd_env_prefix_uri(env, name1), eg.data));

  // Test setting a prefix from a relative URI
  assert(serd_env_set_prefix(env, name2, rel) == SERD_BAD_ARG);
  assert(!serd_env_set_base_uri(env, base));
  assert(!serd_env_set_prefix(env, name2, rel));

  // Test setting a prefix from strings
  assert(
    !serd_env_set_prefix(env, zix_string("eg.3"), zix_string(NS_EG "three")));

  serd_env_free(env);
}

static void
test_expand_curie(void)
{
  static const ZixStringView name  = ZIX_STATIC_STRING("eg.1");
  static const ZixStringView eg    = ZIX_STATIC_STRING(NS_EG);
  static const ZixStringView curie = ZIX_STATIC_STRING("eg.1:foo");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, name, eg));

  ZixStringView prefix = zix_empty_string();
  ZixStringView suffix = zix_empty_string();
  assert(!serd_env_expand(env, curie, &prefix, &suffix));
  assert(prefix.data);
  assert(expect_string_view(prefix, NS_EG));
  assert(suffix.data);
  assert(expect_string_view(suffix, "foo"));

  serd_env_free(env);
}

static void
test_expand_bad_curie(void)
{
  static const ZixStringView prefixed   = ZIX_STATIC_STRING("eg:foo");
  static const ZixStringView unprefixed = ZIX_STATIC_STRING("bar");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  ZixStringView prefix = zix_empty_string();
  ZixStringView suffix = zix_empty_string();
  assert(serd_env_expand(env, prefixed, &prefix, &suffix) == SERD_BAD_CURIE);
  assert(!prefix.length);
  assert(!suffix.length);

  assert(serd_env_expand(env, unprefixed, &prefix, &suffix) == SERD_BAD_CURIE);
  assert(!prefix.length);
  assert(!suffix.length);

  serd_env_free(env);
}

static void
test_qualify(void)
{
  static const ZixStringView eg = ZIX_STATIC_STRING(NS_EG);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, zix_string("eg"), eg));

  ZixStringView prefix = zix_empty_string();
  ZixStringView suffix = zix_empty_string();
  assert(!serd_env_qualify(env, zix_string(NS_EG "foo"), &prefix, &suffix));
  assert(prefix.length == 2);
  assert(expect_string_view(prefix, "eg"));
  assert(suffix.length == 3);
  assert(expect_string_view(suffix, "foo"));

  assert(serd_env_qualify(
           env, zix_string("http://drobilla.net/bar"), &prefix, &suffix) ==
         SERD_FAILURE);

  serd_env_free(env);
}

int
main(void)
{
  test_new_failed_alloc();
  test_set_base_failed_alloc();
  test_set_prefix_existing_failed_alloc();
  test_set_prefix_absolute_failed_alloc();
  test_set_prefix_relative_failed_alloc();
  test_null();
  test_base_uri();
  test_set_prefix();
  test_expand_curie();
  test_expand_bad_curie();
  test_qualify();
  return 0;
}
