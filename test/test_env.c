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
#include "serd/string_view.h"
#include "serd/world.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NS_EG "http://example.org/"

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world          = serd_world_new(&allocator.base);
  const size_t     n_world_allocs = allocator.n_allocations;

  // Successfully allocate a env to count the number of allocations
  SerdEnv* const env = serd_env_new(world, serd_empty_string());
  assert(env);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_world_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_env_new(world, serd_empty_string()));
  }

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_copy_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world          = serd_world_new(&allocator.base);
  SerdEnv* const   env            = serd_env_new(world, serd_empty_string());
  const size_t     n_world_allocs = allocator.n_allocations;

  // Successfully copy an env to count the number of allocations
  SerdEnv* copy = serd_env_copy(&allocator.base, env);
  assert(copy);

  // Test that each allocation failing is handled gracefully
  const size_t n_copy_allocs = allocator.n_allocations - n_world_allocs;
  for (size_t i = 0; i < n_copy_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_env_copy(&allocator.base, env));
  }

  serd_env_free(copy);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_set_prefix_absolute_failed_alloc(void)
{
  const SerdStringView base_uri = serd_string("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  SerdEnv* const   env   = serd_env_new(world, base_uri);

  char name[64] = "eg";
  char uri[64]  = "http://example.org/";

  SerdStatus   st             = SERD_SUCCESS;
  const size_t n_setup_allocs = allocator.n_allocations;

  // Successfully set an absolute prefix to count the number of allocations
  st = serd_env_set_prefix(env, serd_string(name), serd_string(uri));
  assert(st == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_set_prefix_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_set_prefix_allocs; ++i) {
    allocator.n_remaining = i;

    snprintf(name, sizeof(name), "eg%zu", i);
    snprintf(uri, sizeof(name), "http://example.org/%zu", i);

    st = serd_env_set_prefix(env, serd_string(name), serd_string(uri));
    assert(st == SERD_BAD_ALLOC);
  }

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_set_prefix_relative_failed_alloc(void)
{
  const SerdStringView base_uri = serd_string("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world          = serd_world_new(&allocator.base);
  const size_t     n_setup_allocs = allocator.n_allocations;

  char name[64] = "egX";
  char uri[64]  = "relativeX";

  // Successfully set an absolute prefix to count the number of allocations
  SerdEnv*   env = serd_env_new(world, base_uri);
  SerdStatus st = serd_env_set_prefix(env, serd_string(name), serd_string(uri));
  assert(st == SERD_SUCCESS);
  serd_env_free(env);

  // Test that each allocation failing is handled gracefully
  const size_t n_set_prefix_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_set_prefix_allocs; ++i) {
    allocator.n_remaining = i;

    snprintf(name, sizeof(name), "eg%zu", i);
    snprintf(uri, sizeof(uri), "relative%zu", i);

    env = serd_env_new(world, base_uri);
    if (env) {
      st = serd_env_set_prefix(env, serd_string(name), serd_string(uri));
      assert(st == SERD_BAD_ALLOC);
    }

    serd_env_free(env);
  }

  serd_world_free(world);
}

static void
test_copy(void)
{
  assert(!serd_env_copy(NULL, NULL));

  SerdWorld* const world = serd_world_new(NULL);

  SerdEnv* const env =
    serd_env_new(world, serd_string("http://example.org/base/"));

  serd_env_set_prefix(
    env, serd_string("eg"), serd_string("http://example.org/"));

  SerdEnv* const env_copy = serd_env_copy(serd_world_allocator(world), env);

  assert(serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env_copy, serd_string("test"), serd_string("http://example.org/test"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env, serd_string("test2"), serd_string("http://example.org/test2"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_free(env_copy);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_comparison(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(world, serd_empty_string());

  assert(!serd_env_equals(env, NULL));
  assert(!serd_env_equals(NULL, env));
  assert(serd_env_equals(NULL, NULL));
  assert(serd_env_equals(env, env));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_null(void)
{
  // "Copying" NULL returns null
  assert(!serd_env_copy(NULL, NULL));

  // Accessors are tolerant to a NULL env for convenience
  assert(!serd_env_base_uri(NULL));
  assert(!serd_env_expand_node(NULL, NULL));

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
  SerdWorld* const      world = serd_world_new(NULL);
  SerdNodes* const      nodes = serd_world_nodes(world);
  SerdEnv* const        env   = serd_env_new(world, serd_empty_string());
  const SerdNode* const eg    = serd_nodes_uri(nodes, serd_string(NS_EG));

  // Test that invalid calls work as expected
  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, serd_empty_string()));
  assert(!serd_env_base_uri(env));

  // Try setting a relative prefix with no base URI
  assert(serd_env_set_prefix(env, serd_string("eg.3"), serd_string("rel")) ==
         SERD_BAD_ARG);

  // Set a valid base URI
  assert(!serd_env_set_base_uri(env, serd_node_string_view(eg)));
  assert(serd_node_equals(serd_env_base_uri(env), eg));

  // Reset the base URI
  assert(!serd_env_set_base_uri(env, serd_empty_string()));
  assert(!serd_env_base_uri(env));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_set_prefix(void)
{
  const SerdStringView eg    = serd_string(NS_EG);
  const SerdStringView name1 = serd_string("eg.1");
  const SerdStringView name2 = serd_string("eg.2");
  const SerdStringView rel   = serd_string("rel");
  const SerdStringView base  = serd_string("http://example.org/");

  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(world, serd_empty_string());

  // Set a valid prefix
  assert(!serd_env_set_prefix(env, name1, eg));

  // Test setting a prefix from a relative URI
  assert(serd_env_set_prefix(env, name2, rel) == SERD_BAD_ARG);
  assert(!serd_env_set_base_uri(env, base));
  assert(!serd_env_set_prefix(env, name2, rel));

  // Test setting a prefix from strings
  assert(!serd_env_set_prefix(
    env, serd_string("eg.3"), serd_string("http://example.org/three")));

  size_t          n_prefixes = 0;
  SerdSink* const count_prefixes_sink =
    serd_sink_new(world, &n_prefixes, count_prefixes, NULL);

  serd_env_write_prefixes(env, count_prefixes_sink);
  serd_sink_free(count_prefixes_sink);
  assert(n_prefixes == 3);

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_expand_untyped_literal(void)
{
  SerdWorld* const      world   = serd_world_new(NULL);
  SerdNodes* const      nodes   = serd_world_nodes(world);
  const SerdNode* const untyped = serd_nodes_string(nodes, serd_string("data"));
  SerdEnv* const        env     = serd_env_new(world, serd_empty_string());

  assert(!serd_env_expand_node(env, untyped));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_expand_bad_uri_datatype(void)
{
  const SerdStringView type = serd_string("Type");

  SerdWorld* world = serd_world_new(NULL);
  SerdNodes* nodes = serd_nodes_new(serd_world_allocator(world));

  const SerdNode* const typed =
    serd_nodes_literal(nodes, serd_string("data"), SERD_HAS_DATATYPE, type);

  SerdEnv* const env = serd_env_new(world, serd_empty_string());

  assert(!serd_env_expand_node(env, typed));

  serd_env_free(env);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

static void
test_expand_uri(void)
{
  const SerdStringView base = serd_string("http://example.org/b/");

  SerdWorld* const      world     = serd_world_new(NULL);
  SerdNodes* const      nodes     = serd_world_nodes(world);
  SerdEnv* const        env       = serd_env_new(world, base);
  const SerdNode* const rel       = serd_nodes_uri(nodes, serd_string("rel"));
  SerdNode* const       rel_out   = serd_env_expand_node(env, rel);
  const SerdNode* const empty     = serd_nodes_uri(nodes, serd_empty_string());
  SerdNode* const       empty_out = serd_env_expand_node(env, empty);

  assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
  assert(!strcmp(serd_node_string(empty_out), "http://example.org/b/"));

  serd_node_free(serd_world_allocator(world), empty_out);
  serd_node_free(serd_world_allocator(world), rel_out);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_expand_empty_uri_ref(void)
{
  const SerdStringView base = serd_string("http://example.org/b/");

  SerdWorld* const      world   = serd_world_new(NULL);
  SerdNodes* const      nodes   = serd_world_nodes(world);
  const SerdNode* const rel     = serd_nodes_uri(nodes, serd_string("rel"));
  SerdEnv* const        env     = serd_env_new(world, base);
  SerdNode* const       rel_out = serd_env_expand_node(env, rel);

  assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
  serd_node_free(serd_world_allocator(world), rel_out);

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_expand_bad_uri(void)
{
  SerdWorld* const      world   = serd_world_new(NULL);
  SerdNodes* const      nodes   = serd_world_nodes(world);
  const SerdNode* const bad_uri = serd_nodes_uri(nodes, serd_string("rel"));
  SerdEnv* const        env     = serd_env_new(world, serd_empty_string());

  assert(!serd_env_expand_node(env, bad_uri));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_expand_curie(void)
{
  const SerdStringView name = serd_string("eg.1");
  const SerdStringView eg   = serd_string(NS_EG);

  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(world, serd_empty_string());

  assert(!serd_env_set_prefix(env, name, eg));

  SerdNode* const expanded =
    serd_env_expand_curie(env, serd_string("eg.1:foo"));

  assert(expanded);
  assert(!strcmp(serd_node_string(expanded), "http://example.org/foo"));
  serd_node_free(serd_world_allocator(world), expanded);

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_expand_bad_curie(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(world, serd_empty_string());

  assert(!serd_env_expand_curie(NULL, serd_empty_string()));
  assert(!serd_env_expand_curie(NULL, serd_string("what:ever")));
  assert(!serd_env_expand_curie(env, serd_string("eg.1:foo")));
  assert(!serd_env_expand_curie(env, serd_string("nocolon")));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_expand_blank(void)
{
  SerdWorld* const      world = serd_world_new(NULL);
  SerdNodes* const      nodes = serd_world_nodes(world);
  const SerdNode* const blank =
    serd_nodes_token(nodes, SERD_BLANK, serd_string("b1"));

  SerdEnv* const env = serd_env_new(world, serd_empty_string());

  assert(!serd_env_expand_node(env, blank));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_equals(void)
{
  const SerdStringView name1 = serd_string("n1");
  const SerdStringView base1 = serd_string(NS_EG "b1/");
  const SerdStringView base2 = serd_string(NS_EG "b2/");

  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env1  = serd_env_new(world, base1);
  SerdEnv* const   env2  = serd_env_new(world, base2);

  assert(!serd_env_equals(env1, NULL));
  assert(!serd_env_equals(NULL, env1));
  assert(serd_env_equals(NULL, NULL));
  assert(!serd_env_equals(env1, env2));

  serd_env_set_base_uri(env2, base1);
  assert(serd_env_equals(env1, env2));

  assert(!serd_env_set_prefix(env1, name1, serd_string(NS_EG "n1")));
  assert(!serd_env_equals(env1, env2));
  assert(!serd_env_set_prefix(env2, name1, serd_string(NS_EG "othern1")));
  assert(!serd_env_equals(env1, env2));
  assert(!serd_env_set_prefix(env2, name1, serd_string(NS_EG "n1")));
  assert(serd_env_equals(env1, env2));

  serd_env_set_base_uri(env2, base2);
  assert(!serd_env_equals(env1, env2));

  SerdEnv* const env3 = serd_env_copy(NULL, env2);
  assert(serd_env_equals(env3, env2));
  serd_env_free(env3);

  serd_env_free(env2);
  serd_env_free(env1);
  serd_world_free(world);
}

int
main(void)
{
  test_new_failed_alloc();
  test_copy_failed_alloc();
  test_set_prefix_absolute_failed_alloc();
  test_set_prefix_relative_failed_alloc();
  test_copy();
  test_comparison();
  test_null();
  test_base_uri();
  test_set_prefix();
  test_expand_untyped_literal();
  test_expand_bad_uri_datatype();
  test_expand_uri();
  test_expand_empty_uri_ref();
  test_expand_bad_uri();
  test_expand_curie();
  test_expand_bad_curie();
  test_expand_blank();
  test_equals();
  return 0;
}
