// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/env.h>
#include <serd/node.h>
#include <serd/node_type.h>
#include <serd/status.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdio.h>

#define NS_EG "http://example.org/"

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a env to count the number of allocations
  SerdEnv* const env = serd_env_new(&allocator.base, NULL);
  assert(env);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_env_new(&allocator.base, NULL));
  }

  serd_env_free(env);
}

static void
test_set_prefix_failed_alloc(void)
{
  SerdNode name = serd_node_from_string(SERD_LITERAL, "eg");
  SerdNode uri  = serd_node_from_string(SERD_URI, "http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env       = serd_env_new(&allocator.base, NULL);
  SerdStatus           st        = SERD_SUCCESS;

  serd_failing_allocator_reset(&allocator, 2);

  st = serd_env_set_prefix(env, &name, &uri);
  assert(st == SERD_BAD_ALLOC);

  serd_env_free(env);
}

static SerdStatus
count_prefixes(void* const           handle,
               const SerdNode* const name,
               const SerdNode* const uri)
{
  (void)name;
  (void)uri;

  ++*(int*)handle;
  return SERD_SUCCESS;
}

static void
test_env(void)
{
  SerdNode u   = serd_node_from_string(SERD_URI, NS_EG "foo");
  SerdNode b   = serd_node_from_string(SERD_CURIE, "invalid");
  SerdNode c   = serd_node_from_string(SERD_CURIE, "eg.2:b");
  SerdEnv* env = serd_env_new(NULL, NULL);
  serd_env_set_prefix_from_strings(env, "eg.2", NS_EG "");

  assert(!serd_env_set_base_uri(env, NULL));
  assert(serd_env_set_base_uri(env, &SERD_NODE_NULL));
  assert(serd_node_equals(serd_env_base_uri(env, NULL), &SERD_NODE_NULL));

  ZixStringView prefix;
  ZixStringView suffix;
  assert(!serd_env_qualify(NULL, &u, &u, &suffix));
  assert(serd_env_expand(NULL, &c, &prefix, &suffix));
  assert(serd_env_expand(env, &b, &prefix, &suffix) == SERD_BAD_ARG);
  assert(serd_env_expand(env, &u, &prefix, &suffix) == SERD_BAD_ARG);

  SerdNode nxnode = serd_env_expand_node(NULL, &c);
  assert(serd_node_equals(&nxnode, &SERD_NODE_NULL));

  SerdNode xnode = serd_env_expand_node(env, &SERD_NODE_NULL);
  assert(serd_node_equals(&xnode, &SERD_NODE_NULL));

  SerdNode xu = serd_env_expand_node(env, &u);
  assert(expect_string(xu.buf, NS_EG "foo"));
  serd_node_free(NULL, &xu);

  SerdNode badpre  = serd_node_from_string(SERD_CURIE, "hm:what");
  SerdNode xbadpre = serd_env_expand_node(env, &badpre);
  assert(serd_node_equals(&xbadpre, &SERD_NODE_NULL));

  SerdNode xc = serd_env_expand_node(env, &c);
  assert(expect_string(xc.buf, NS_EG "b"));
  serd_node_free(NULL, &xc);

  assert(serd_env_set_prefix(env, &SERD_NODE_NULL, &SERD_NODE_NULL));

  const SerdNode lit = serd_node_from_string(SERD_LITERAL, "hello");
  assert(serd_env_set_prefix(env, &b, &lit));

  assert(!serd_env_new(NULL, &lit));

  const SerdNode blank  = serd_node_from_string(SERD_BLANK, "b1");
  const SerdNode xblank = serd_env_expand_node(env, &blank);
  assert(serd_node_equals(&xblank, &SERD_NODE_NULL));

  int n_prefixes = 0;
  serd_env_set_prefix_from_strings(env, "eg.2", NS_EG "");
  serd_env_foreach(env, count_prefixes, &n_prefixes);
  assert(n_prefixes == 1);

  SerdNode shorter_uri = serd_node_from_string(SERD_URI, "urn:foo");
  SerdNode prefix_name;
  assert(!serd_env_qualify(env, &shorter_uri, &prefix_name, &suffix));

  assert(!serd_env_set_base_uri(env, &u));
  assert(serd_node_equals(serd_env_base_uri(env, NULL), &u));
  assert(!serd_env_set_base_uri(env, NULL));
  assert(!serd_env_base_uri(env, NULL)->buf);

  serd_env_free(env);
}

int
main(void)
{
  test_new_failed_alloc();
  test_set_prefix_failed_alloc();
  test_env();
  return 0;
}
