// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
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

  SerdEnv* const env = serd_env_new(&allocator.base, zix_empty_string());

  assert(!serd_env_set_prefix(env, zix_string(name), zix_string(uri)));
  assert(!serd_env_set_base_uri(env, zix_string(uri)));

  // Successfully copy an env to count the number of allocations
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

  serd_env_set_prefix(env, zix_string("eg"), zix_string("http://example.org/"));

  SerdEnv* const env_copy = serd_env_copy(NULL, env);

  assert(serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env_copy, zix_string("test"), zix_string("http://example.org/test"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env, zix_string("test2"), zix_string("http://example.org/test2"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_free(env_copy);
  serd_env_free(env);
}

static void
test_comparison(void)
{
  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_equals(env, NULL));
  assert(!serd_env_equals(NULL, env));
  assert(serd_env_equals(NULL, NULL));
  assert(serd_env_equals(env, env));

  serd_env_free(env);
}

static void
test_null(void)
{
  SerdNode* const eg = serd_new_uri(NULL, zix_string(NS_EG));

  // "Copying" NULL returns null
  assert(!serd_env_copy(NULL, NULL));

  // Accessors are tolerant to a NULL env for convenience
  assert(!serd_env_base_uri(NULL));
  assert(!serd_env_expand_node(NULL, NULL));
  assert(!serd_env_qualify(NULL, eg));

  // Only null is equal to null
  assert(serd_env_equals(NULL, NULL));

  serd_node_free(NULL, eg);
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
  SerdEnv* const  env = serd_env_new(NULL, zix_empty_string());
  SerdNode* const eg  = serd_new_uri(NULL, zix_string(NS_EG));

  // Test that invalid calls work as expected
  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_env_base_uri(env));

  // Try setting a relative prefix with no base URI
  assert(serd_env_set_prefix(env, zix_string("eg.3"), zix_string("rel")) ==
         SERD_BAD_ARG);

  // Set a valid base URI
  assert(!serd_env_set_base_uri(env, serd_node_string_view(eg)));
  assert(serd_node_equals(serd_env_base_uri(env), eg));

  // Reset the base URI
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_env_base_uri(env));

  serd_node_free(NULL, eg);
  serd_env_free(env);
}

static void
test_set_prefix(void)
{
  static const ZixStringView eg    = ZIX_STATIC_STRING(NS_EG);
  static const ZixStringView name1 = ZIX_STATIC_STRING("eg.1");
  static const ZixStringView name2 = ZIX_STATIC_STRING("eg.2");
  static const ZixStringView rel   = ZIX_STATIC_STRING("rel");
  static const ZixStringView base  = ZIX_STATIC_STRING("http://example.org/");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  // Set a valid prefix
  assert(!serd_env_set_prefix(env, name1, eg));

  // Test setting a prefix from a relative URI
  assert(serd_env_set_prefix(env, name2, rel) == SERD_BAD_ARG);
  assert(!serd_env_set_base_uri(env, base));
  assert(!serd_env_set_prefix(env, name2, rel));

  // Test setting a prefix from strings
  assert(!serd_env_set_prefix(
    env, zix_string("eg.3"), zix_string("http://example.org/three")));

  size_t          n_prefixes = 0;
  SerdSink* const count_prefixes_sink =
    serd_sink_new(NULL, &n_prefixes, count_prefixes, NULL);

  serd_env_write_prefixes(env, count_prefixes_sink);
  serd_sink_free(count_prefixes_sink);
  assert(n_prefixes == 3);

  serd_env_free(env);
}

static void
test_expand_untyped_literal(void)
{
  SerdNode* const untyped = serd_new_string(NULL, zix_string("data"));
  SerdEnv* const  env     = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_expand_node(env, untyped));

  serd_env_free(env);
  serd_node_free(NULL, untyped);
}

static void
test_expand_bad_uri_datatype(void)
{
  static const ZixStringView type = ZIX_STATIC_STRING("Type");

  SerdNode* const typed =
    serd_new_literal(NULL, zix_string("data"), SERD_HAS_DATATYPE, type);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_expand_node(env, typed));

  serd_env_free(env);
  serd_node_free(NULL, typed);
}

static void
test_expand_uri(void)
{
  static const ZixStringView base = ZIX_STATIC_STRING("http://example.org/b/");

  SerdEnv* const  env       = serd_env_new(NULL, base);
  SerdNode* const rel       = serd_new_uri(NULL, zix_string("rel"));
  SerdNode* const rel_out   = serd_env_expand_node(env, rel);
  SerdNode* const empty     = serd_new_uri(NULL, zix_empty_string());
  SerdNode* const empty_out = serd_env_expand_node(env, empty);

  assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
  assert(!strcmp(serd_node_string(empty_out), "http://example.org/b/"));

  serd_node_free(NULL, empty_out);
  serd_node_free(NULL, empty);
  serd_node_free(NULL, rel_out);
  serd_node_free(NULL, rel);
  serd_env_free(env);
}

static void
test_expand_empty_uri_ref(void)
{
  static const ZixStringView base = ZIX_STATIC_STRING("http://example.org/b/");

  SerdNode* const rel     = serd_new_uri(NULL, zix_string("rel"));
  SerdEnv* const  env     = serd_env_new(NULL, base);
  SerdNode* const rel_out = serd_env_expand_node(env, rel);

  assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
  serd_node_free(NULL, rel_out);

  serd_env_free(env);
  serd_node_free(NULL, rel);
}

static void
test_expand_bad_uri(void)
{
  SerdNode* const bad_uri = serd_new_uri(NULL, zix_string("rel"));
  SerdEnv* const  env     = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_expand_node(env, bad_uri));

  serd_env_free(env);
  serd_node_free(NULL, bad_uri);
}

static void
test_expand_curie(void)
{
  static const ZixStringView name = ZIX_STATIC_STRING("eg.1");
  static const ZixStringView eg   = ZIX_STATIC_STRING(NS_EG);

  SerdNode* const curie = serd_new_curie(NULL, zix_string("eg.1:foo"));
  SerdEnv* const  env   = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, name, eg));

  SerdNode* const curie_out = serd_env_expand_node(env, curie);
  assert(curie_out);
  assert(!strcmp(serd_node_string(curie_out), "http://example.org/foo"));
  serd_node_free(NULL, curie_out);

  serd_env_free(env);
  serd_node_free(NULL, curie);
}

static void
test_expand_bad_curie(void)
{
  SerdNode* const name  = serd_new_uri(NULL, zix_string("name"));
  SerdNode* const curie = serd_new_curie(NULL, zix_string("eg.1:foo"));
  SerdEnv* const  env   = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_expand_node(env, name));
  assert(!serd_env_expand_node(env, curie));

  serd_env_free(env);
  serd_node_free(NULL, curie);
  serd_node_free(NULL, name);
}

static void
test_expand_blank(void)
{
  SerdNode* const blank = serd_new_blank(NULL, zix_string("b1"));
  SerdEnv* const  env   = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_expand_node(env, blank));

  serd_env_free(env);
  serd_node_free(NULL, blank);
}

static void
test_qualify(void)
{
  const ZixStringView eg = zix_string(NS_EG);

  SerdNode* const name = serd_new_string(NULL, zix_string("eg"));
  SerdNode* const c1   = serd_new_curie(NULL, zix_string("eg:foo"));
  SerdNode* const u1 = serd_new_uri(NULL, zix_string("http://example.org/foo"));
  SerdNode* const u2 =
    serd_new_uri(NULL, zix_string("http://drobilla.net/bar"));

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, serd_node_string_view(name), eg));

  assert(!serd_env_expand_node(env, name));

  SerdNode* const u1_out = serd_env_qualify(env, u1);
  assert(serd_node_equals(u1_out, c1));
  serd_node_free(NULL, u1_out);

  assert(!serd_env_qualify(env, u2));

  serd_env_free(env);
  serd_node_free(NULL, u2);
  serd_node_free(NULL, u1);
  serd_node_free(NULL, c1);
  serd_node_free(NULL, name);
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
  test_qualify();
  test_equals();
  return 0;
}
