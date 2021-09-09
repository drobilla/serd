// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/string_view.h"

#include <assert.h>
#include <string.h>

#define NS_EG "http://example.org/"

static void
test_copy(void)
{
  assert(!serd_env_copy(NULL));

  SerdEnv* const env = serd_env_new(serd_string("http://example.org/base/"));

  serd_env_set_prefix(
    env, serd_string("eg"), serd_string("http://example.org/"));

  SerdEnv* const env_copy = serd_env_copy(env);

  assert(serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env_copy, serd_string("test"), serd_string("http://example.org/test"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env, serd_string("test2"), serd_string("http://example.org/test2"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_free(env_copy);
  serd_env_free(env);
}

static void
test_comparison(void)
{
  SerdEnv* const env = serd_env_new(serd_empty_string());

  assert(!serd_env_equals(env, NULL));
  assert(!serd_env_equals(NULL, env));
  assert(serd_env_equals(NULL, NULL));
  assert(serd_env_equals(env, env));

  serd_env_free(env);
}

static void
test_null(void)
{
  // "Copying" NULL returns null
  assert(!serd_env_copy(NULL));

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
  SerdEnv* const  env = serd_env_new(serd_empty_string());
  SerdNode* const eg  = serd_new_uri(serd_string(NS_EG));

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
  serd_node_free(eg);
}

static void
test_set_prefix(void)
{
  const SerdStringView eg    = serd_string(NS_EG);
  const SerdStringView name1 = serd_string("eg.1");
  const SerdStringView name2 = serd_string("eg.2");
  const SerdStringView rel   = serd_string("rel");
  const SerdStringView base  = serd_string("http://example.org/");

  SerdEnv* const env = serd_env_new(serd_empty_string());

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
    serd_sink_new(&n_prefixes, count_prefixes, NULL);

  serd_env_write_prefixes(env, count_prefixes_sink);
  serd_sink_free(count_prefixes_sink);
  assert(n_prefixes == 3);

  serd_env_free(env);
}

static void
test_expand_untyped_literal(void)
{
  SerdNode* const untyped = serd_new_string(serd_string("data"));
  SerdEnv* const  env     = serd_env_new(serd_empty_string());

  assert(!serd_env_expand_node(env, untyped));

  serd_env_free(env);
  serd_node_free(untyped);
}

static void
test_expand_bad_uri_datatype(void)
{
  const SerdStringView type = serd_string("Type");

  SerdNodes* nodes = serd_nodes_new();

  const SerdNode* const typed =
    serd_nodes_literal(nodes, serd_string("data"), SERD_HAS_DATATYPE, type);

  SerdEnv* const env = serd_env_new(serd_empty_string());

  assert(!serd_env_expand_node(env, typed));

  serd_env_free(env);
  serd_nodes_free(nodes);
}

static void
test_expand_uri(void)
{
  const SerdStringView base = serd_string("http://example.org/b/");

  SerdEnv* const  env       = serd_env_new(base);
  SerdNode* const rel       = serd_new_uri(serd_string("rel"));
  SerdNode* const rel_out   = serd_env_expand_node(env, rel);
  SerdNode* const empty     = serd_new_uri(serd_empty_string());
  SerdNode* const empty_out = serd_env_expand_node(env, empty);

  assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
  assert(!strcmp(serd_node_string(empty_out), "http://example.org/b/"));

  serd_node_free(empty_out);
  serd_node_free(empty);
  serd_node_free(rel_out);
  serd_node_free(rel);
  serd_env_free(env);
}

static void
test_expand_empty_uri_ref(void)
{
  const SerdStringView base = serd_string("http://example.org/b/");

  SerdNode* const rel     = serd_new_uri(serd_string("rel"));
  SerdEnv* const  env     = serd_env_new(base);
  SerdNode* const rel_out = serd_env_expand_node(env, rel);

  assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
  serd_node_free(rel_out);

  serd_env_free(env);
  serd_node_free(rel);
}

static void
test_expand_bad_uri(void)
{
  SerdNode* const bad_uri = serd_new_uri(serd_string("rel"));
  SerdEnv* const  env     = serd_env_new(serd_empty_string());

  assert(!serd_env_expand_node(env, bad_uri));

  serd_env_free(env);
  serd_node_free(bad_uri);
}

static void
test_expand_curie(void)
{
  const SerdStringView name = serd_string("eg.1");
  const SerdStringView eg   = serd_string(NS_EG);

  SerdEnv* const env = serd_env_new(serd_empty_string());

  assert(!serd_env_set_prefix(env, name, eg));

  SerdNode* const expanded =
    serd_env_expand_curie(env, serd_string("eg.1:foo"));

  assert(expanded);
  assert(!strcmp(serd_node_string(expanded), "http://example.org/foo"));
  serd_node_free(expanded);

  serd_env_free(env);
}

static void
test_expand_bad_curie(void)
{
  SerdEnv* const env = serd_env_new(serd_empty_string());

  assert(!serd_env_expand_curie(NULL, serd_empty_string()));
  assert(!serd_env_expand_curie(NULL, serd_string("what:ever")));
  assert(!serd_env_expand_curie(env, serd_string("eg.1:foo")));
  assert(!serd_env_expand_curie(env, serd_string("nocolon")));

  serd_env_free(env);
}

static void
test_expand_blank(void)
{
  SerdNode* const blank = serd_new_token(SERD_BLANK, serd_string("b1"));
  SerdEnv* const  env   = serd_env_new(serd_empty_string());

  assert(!serd_env_expand_node(env, blank));

  serd_env_free(env);
  serd_node_free(blank);
}

static void
test_equals(void)
{
  const SerdStringView name1 = serd_string("n1");
  const SerdStringView base1 = serd_string(NS_EG "b1/");
  const SerdStringView base2 = serd_string(NS_EG "b2/");

  SerdEnv* const env1 = serd_env_new(base1);
  SerdEnv* const env2 = serd_env_new(base2);

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

  SerdEnv* const env3 = serd_env_copy(env2);
  assert(serd_env_equals(env3, env2));
  serd_env_free(env3);

  serd_env_free(env2);
  serd_env_free(env1);
}

int
main(void)
{
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
