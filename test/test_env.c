// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NS_EG "http://example.org/"

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a env to count the number of allocations
  SerdEnv* const env = serd_env_new(&allocator.base, zix_empty_string());
  assert(env);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_env_new(&allocator.base, zix_empty_string()));
  }

  serd_env_free(env);
}

static void
test_copy_failed_alloc(void)
{
  static const char name[] = "eg";
  static const char uri[]  = "http://example.org/";

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Create a simple env
  SerdEnv* const env = serd_env_new(&allocator.base, zix_empty_string());
  assert(!serd_env_set_prefix(env, zix_string(name), zix_string(uri)));
  assert(!serd_env_set_base_uri(env, zix_string(uri)));

  // Successfully copy the env to count the number of allocations
  const size_t   n_setup_allocs = allocator.n_allocations;
  SerdEnv* const copy           = serd_env_copy(&allocator.base, env);
  assert(copy);

  // Test that each allocation failing is handled gracefully
  const size_t n_copy_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_copy_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_env_copy(&allocator.base, env));
  }

  serd_env_free(copy);
  serd_env_free(env);
}

static void
test_set_prefix_absolute_failed_alloc(void)
{
  static const ZixStringView base_uri =
    ZIX_STATIC_STRING("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdEnv* const env = serd_env_new(&allocator.base, base_uri);

  char name[64] = "eg";
  char uri[64]  = "http://example.org/";

  SerdStatus   st             = SERD_SUCCESS;
  const size_t n_setup_allocs = allocator.n_allocations;

  // Successfully set an absolute prefix to count the number of allocations
  st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
  assert(st == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_set_prefix_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_set_prefix_allocs; ++i) {
    allocator.n_remaining = i;

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

  char name[64] = "egX";
  char uri[64]  = "relativeX";

  // Successfully set an absolute prefix to count the number of allocations
  SerdEnv*   env = serd_env_new(&allocator.base, base_uri);
  SerdStatus st  = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
  assert(st == SERD_SUCCESS);
  serd_env_free(env);

  // Test that each allocation failing is handled gracefully
  const size_t n_set_prefix_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_set_prefix_allocs; ++i) {
    allocator.n_remaining = i;

    snprintf(name, sizeof(name), "eg%zu", i);
    snprintf(uri, sizeof(uri), "relative%zu", i);

    env = serd_env_new(&allocator.base, base_uri);
    if (env) {
      st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
      assert(st == SERD_BAD_ALLOC);
    }

    serd_env_free(env);
  }
}

static void
test_copy(void)
{
  assert(!serd_env_copy(NULL, NULL));

  SerdEnv* const env =
    serd_env_new(NULL, zix_string("http://example.org/base/"));

  serd_env_set_prefix(env, zix_string("eg"), zix_string(NS_EG));

  SerdEnv* const env_copy = serd_env_copy(NULL, env);
  assert(serd_env_equals(env, env_copy));

  serd_env_set_prefix(env_copy, zix_string("test"), zix_string(NS_EG "test"));
  assert(!serd_env_equals(env, env_copy));

  serd_env_set_prefix(env, zix_string("test"), zix_string(NS_EG "test"));
  assert(serd_env_equals(env, env_copy));

  serd_env_set_prefix(env, zix_string("test2"), zix_string(NS_EG "test2"));
  assert(!serd_env_equals(env, env_copy));

  serd_env_free(env_copy);
  serd_env_free(env);
}

static void
test_equals(void)
{
  static const ZixStringView name1 = ZIX_STATIC_STRING("n1");
  static const ZixStringView base1 = ZIX_STATIC_STRING(NS_EG "b1/");
  static const ZixStringView base2 = ZIX_STATIC_STRING(NS_EG "b2/");

  SerdEnv* const env1 = serd_env_new(NULL, base1);
  SerdEnv* const env2 = serd_env_new(NULL, base2);

  assert(!serd_env_equals(env1, NULL));
  assert(!serd_env_equals(NULL, env1));
  assert(serd_env_equals(NULL, NULL));
  assert(!serd_env_equals(env1, env2));

  serd_env_set_base_uri(env2, base1);
  assert(serd_env_equals(env1, env2));

  assert(!serd_env_set_prefix(env1, name1, zix_string(NS_EG "n1")));
  assert(!serd_env_equals(env1, env2));
  assert(!serd_env_set_prefix(env2, name1, zix_string(NS_EG "othern1")));
  assert(!serd_env_equals(env1, env2));
  assert(!serd_env_set_prefix(env2, name1, zix_string(NS_EG "n1")));
  assert(serd_env_equals(env1, env2));

  serd_env_set_base_uri(env2, base2);
  assert(!serd_env_equals(env1, env2));

  SerdEnv* const env3 = serd_env_copy(NULL, env2);
  assert(serd_env_equals(env3, env2));
  serd_env_free(env3);

  serd_env_free(env2);
  serd_env_free(env1);
}

static void
test_null(void)
{
  // "Copying" NULL returns null
  assert(!serd_env_copy(NULL, NULL));

  // Accessors are tolerant to a NULL env for convenience
  ZixStringView prefix = {NULL, 0U};
  ZixStringView suffix = {NULL, 0U};
  assert(!serd_env_base_uri_view(NULL).scheme.length);
  assert(!serd_env_get_prefix(NULL, zix_string("name")).length);
  assert(serd_env_expand(NULL, zix_empty_string(), &prefix, &suffix) ==
         SERD_FAILURE);
  assert(serd_env_qualify(NULL, zix_empty_string(), &prefix, &suffix) ==
         SERD_FAILURE);

  // Only null is equal to null
  assert(serd_env_equals(NULL, NULL));
}

static SerdStatus
count_prefixes(void* handle, const SerdEvent* event)
{
  *(int*)handle += event->type == SERD_PREFIX;

  return SERD_SUCCESS;
}

static void
test_base_uri(void)
{
  assert(!serd_env_new(NULL, zix_string("rel")));

  SerdNodes* const      nodes = serd_nodes_new(NULL);
  SerdEnv* const        env   = serd_env_new(NULL, zix_empty_string());
  const SerdNode* const eg    = serd_nodes_get(nodes, serd_a_uri_string(NS_EG));

  // Test that empty/unset base works as expected
  assert(!serd_env_base_uri_view(env).scheme.length);
  assert(!serd_env_base_uri_string(env).length);
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_env_base_uri_view(env).scheme.length);
  assert(!serd_env_base_uri_string(env).length);

  // Try setting a relative base with no previous base URI
  assert(serd_env_set_base_uri(env, zix_string("rel")) == SERD_BAD_ARG);

  // Try setting a relative prefix with no base URI
  assert(serd_env_set_prefix(env, zix_string("eg.3"), zix_string("rel")) ==
         SERD_BAD_ARG);

  // Set a valid base URI
  assert(!serd_env_set_base_uri(env, serd_node_string_view(eg)));
  assert(!strcmp(serd_env_base_uri_string(env).data, NS_EG));

  // Reset the base URI
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_env_base_uri_view(env).scheme.length);

  serd_env_free(env);
  serd_nodes_free(nodes);
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
  assert(!serd_env_get_prefix(env, name1).length);
  assert(serd_env_get_prefix(env, name1).data);
  assert(serd_env_get_prefix(env, name1).data[0] == '\0');

  // Set a valid prefix
  assert(!serd_env_set_prefix(env, name1, eg));
  assert(!strcmp(serd_env_get_prefix(env, name1).data, eg.data));

  // Test setting a prefix from a relative URI
  assert(serd_env_set_prefix(env, name2, rel) == SERD_BAD_ARG);
  assert(!serd_env_set_base_uri(env, base));
  assert(!serd_env_set_prefix(env, name2, rel));

  // Test setting a prefix from strings
  assert(
    !serd_env_set_prefix(env, zix_string("eg.3"), zix_string(NS_EG "three")));

  size_t          n_prefixes = 0;
  SerdSink* const count_prefixes_sink =
    serd_sink_new(NULL, &n_prefixes, count_prefixes, NULL);

  serd_env_describe(env, count_prefixes_sink);
  serd_sink_free(count_prefixes_sink);
  assert(n_prefixes == 3);

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
  assert(!strcmp(prefix.data, NS_EG));
  assert(suffix.data);
  assert(!strcmp(suffix.data, "foo"));

  serd_env_free(env);
}

static void
test_expand_bad_curie(void)
{
  static const ZixStringView prefixed = ZIX_STATIC_STRING("eg:foo");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  ZixStringView prefix = zix_empty_string();
  ZixStringView suffix = zix_empty_string();
  assert(serd_env_expand(env, prefixed, &prefix, &suffix) == SERD_BAD_CURIE);
  assert(!prefix.length);
  assert(!suffix.length);

  serd_env_free(env);
}

static void
test_qualify(void)
{
  static const ZixStringView eg = ZIX_STATIC_STRING(NS_EG);

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNode* const name = serd_nodes_get(nodes, serd_a_string("eg"));
  const SerdNode* const u1 =
    serd_nodes_get(nodes, serd_a_uri_string(NS_EG "foo"));
  const SerdNode* const u2 =
    serd_nodes_get(nodes, serd_a_uri_string("http://drobilla.net/bar"));

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, serd_node_string_view(name), eg));

  ZixStringView prefix = zix_empty_string();
  ZixStringView suffix = zix_empty_string();
  assert(!serd_env_qualify(env, serd_node_string_view(u1), &prefix, &suffix));
  assert(prefix.length == 2);
  assert(!strncmp(prefix.data, "eg", prefix.length));
  assert(suffix.length == 3);
  assert(!strncmp(suffix.data, "foo", suffix.length));

  assert(serd_env_qualify(env, serd_node_string_view(u2), &prefix, &suffix) ==
         SERD_FAILURE);

  serd_env_free(env);
  serd_nodes_free(nodes);
}

static void
test_sink(void)
{
  SerdNode* const base = serd_node_new(NULL, serd_a_uri_string(NS_EG));
  SerdNode* const name = serd_node_new(NULL, serd_a_string("eg"));
  SerdNode* const uri  = serd_node_new(NULL, serd_a_uri_string(NS_EG "uri"));
  SerdEnv* const  env  = serd_env_new(NULL, zix_empty_string());

  const SerdSink* const sink = serd_env_sink(env);

  assert(!serd_sink_write_base(sink, base));
  assert(!strcmp(serd_env_base_uri_string(env).data, NS_EG));

  assert(!serd_sink_write_prefix(sink, name, uri));

  assert(serd_env_get_prefix(env, zix_string("eg")).length ==
         serd_node_length(uri));
  assert(!strcmp(serd_env_get_prefix(env, zix_string("eg")).data,
                 serd_node_string(uri)));

  assert(!strcmp(serd_env_base_uri_string(env).data, NS_EG));

  serd_env_free(env);
  serd_node_free(NULL, uri);
  serd_node_free(NULL, name);
  serd_node_free(NULL, base);
}

int
main(void)
{
  test_new_failed_alloc();
  test_copy_failed_alloc();
  test_set_prefix_absolute_failed_alloc();
  test_set_prefix_relative_failed_alloc();
  test_copy();
  test_equals();
  test_null();
  test_base_uri();
  test_set_prefix();
  test_expand_curie();
  test_expand_bad_curie();
  test_qualify();
  test_sink();
  return 0;
}
