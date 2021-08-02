// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/env.h>
#include <serd/event.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/string_pair_view.h>
#include <serd/token_view.h>
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
  SerdStringPairView pair = {{"", 0}, {"", 0}};
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(NULL)));
  assert(!serd_env_prefix_uri(NULL, zix_string("name")).length);
  assert(serd_env_expand(NULL, zix_empty_string(), &pair) == SERD_BAD_ARG);
  assert(serd_env_qualify(NULL, zix_empty_string(), &pair) == SERD_BAD_ARG);
}

static SerdStatus
count_prefixes(void* const handle, const SerdEvent* const event)
{
  *(size_t*)handle += event->type == SERD_EVENT_PREFIX;

  return SERD_SUCCESS;
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

  // Set a valid base path
  assert(!serd_env_set_base_path(env, zix_string("/d/f")));
  assert(expect_string_view(serd_env_base_uri_string(env), "file:///d/f"));

  // Reset the base path
  assert(!serd_env_set_base_path(env, zix_empty_string()));
  assert(expect_string_view(serd_env_base_uri_string(env), ""));

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

  // Count prefixes
  size_t         count = 0;
  const SerdSink sink  = {&count, count_prefixes};
  serd_env_write_prefixes(env, &sink);
  assert(count == 3);

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

  SerdStringPairView pair = {{"", 0}, {"", 0}};
  assert(!serd_env_expand(env, curie, &pair));
  assert(pair.prefix.data);
  assert(expect_string_view(pair.prefix, NS_EG));
  assert(pair.suffix.data);
  assert(expect_string_view(pair.suffix, "foo"));

  serd_env_free(env);
}

static void
test_expand_bad_curie(void)
{
  static const ZixStringView prefixed   = ZIX_STATIC_STRING("eg:foo");
  static const ZixStringView unprefixed = ZIX_STATIC_STRING("bar");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  SerdStringPairView pair = {{"", 0}, {"", 0}};
  assert(serd_env_expand(env, prefixed, &pair) == SERD_BAD_CURIE);
  assert(!pair.prefix.length);
  assert(!pair.suffix.length);

  assert(serd_env_expand(env, unprefixed, &pair) == SERD_BAD_CURIE);
  assert(!pair.prefix.length);
  assert(!pair.suffix.length);

  serd_env_free(env);
}

static void
test_qualify(void)
{
  static const ZixStringView eg = ZIX_STATIC_STRING(NS_EG);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, zix_string("eg"), eg));

  SerdStringPairView pair = {zix_empty_string(), zix_empty_string()};
  assert(!serd_env_qualify(env, zix_string(NS_EG "foo"), &pair));
  assert(pair.prefix.length == 2);
  assert(expect_string_view(pair.prefix, "eg"));
  assert(pair.suffix.length == 3);
  assert(expect_string_view(pair.suffix, "foo"));

  assert(serd_env_qualify(env, zix_string("http://drobilla.net/bar"), &pair) ==
         SERD_FAILURE);

  serd_env_free(env);
}

static void
test_sink(void)
{
  static ZixStringView base = ZIX_STATIC_STRING(NS_EG);
  static ZixStringView name = ZIX_STATIC_STRING("eg");
  static ZixStringView uri  = ZIX_STATIC_STRING(NS_EG "uri");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  const SerdSink* const sink = serd_env_sink(env);

  assert(!serd_sink_event(
    sink,
    serd_statement_event(0U,
                         serd_triple_view(serd_token_view(SERD_URI, uri),
                                          serd_token_view(SERD_URI, uri),
                                          serd_token_object_view(
                                            serd_token_view(SERD_URI, uri))))));

  assert(!serd_sink_event(sink, serd_base_event(base)));
  assert(expect_string_view(serd_env_base_uri_string(env), NS_EG));

  assert(!serd_sink_event(sink, serd_prefix_event(name, uri)));

  const ZixStringView eg_prefix = serd_env_prefix_uri(env, zix_string("eg"));
  assert(zix_string_view_equals(eg_prefix, uri));

  assert(expect_string_view(serd_env_base_uri_string(env), NS_EG));

  serd_env_free(env);
}

static SerdStatus
on_describe_event(void* const handle, const SerdEvent* const event)
{
  (void)handle;
  (void)event;
  return SERD_BAD_LITERAL; // A status not used internally by the reader
}

static void
test_describe(void)
{
  static ZixStringView name = ZIX_STATIC_STRING("eg");
  static ZixStringView uri  = ZIX_STATIC_STRING(NS_EG "uri");

  SerdEnv* const        env      = serd_env_new(NULL, uri);
  const SerdSink* const env_sink = serd_env_sink(env);

  assert(!serd_sink_event(env_sink, serd_prefix_event(name, uri)));

  const SerdSink   describe_sink = {NULL, on_describe_event};
  const SerdStatus st            = serd_env_write_prefixes(env, &describe_sink);
  assert(st == SERD_BAD_LITERAL);

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
  test_sink();
  test_describe();
  return 0;
}
