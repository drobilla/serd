/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <string.h>

#define NS_EG "http://example.org/"

static void
test_copy(void)
{
  assert(!serd_env_copy(NULL));

  SerdEnv* const env = serd_env_new(SERD_STRING("http://example.org/base/"));

  serd_env_set_prefix(
    env, SERD_STRING("eg"), SERD_STRING("http://example.org/"));

  SerdEnv* const env_copy = serd_env_copy(env);

  assert(serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env_copy, SERD_STRING("test"), SERD_STRING("http://example.org/test"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_set_prefix(
    env, SERD_STRING("test2"), SERD_STRING("http://example.org/test2"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_free(env_copy);
  serd_env_free(env);
}

static void
test_comparison(void)
{
  SerdEnv* const env = serd_env_new(SERD_EMPTY_STRING());

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
  SerdEnv* const  env = serd_env_new(SERD_EMPTY_STRING());
  SerdNode* const eg  = serd_new_uri(SERD_STRING(NS_EG));

  // Test that invalid calls work as expected
  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));
  assert(!serd_env_base_uri(env));

  // Try setting a relative prefix with no base URI
  assert(serd_env_set_prefix(env, SERD_STRING("eg.3"), SERD_STRING("rel")) ==
         SERD_ERR_BAD_ARG);

  // Set a valid base URI
  assert(!serd_env_set_base_uri(env, serd_node_string_view(eg)));
  assert(serd_node_equals(serd_env_base_uri(env), eg));

  // Reset the base URI
  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));
  assert(!serd_env_base_uri(env));

  serd_env_free(env);
  serd_node_free(eg);
}

static void
test_set_prefix(void)
{
  static const SerdStringView eg    = SERD_STRING(NS_EG);
  static const SerdStringView name1 = SERD_STRING("eg.1");
  static const SerdStringView name2 = SERD_STRING("eg.2");
  static const SerdStringView rel   = SERD_STRING("rel");
  static const SerdStringView base  = SERD_STRING("http://example.org/");

  SerdEnv* const env = serd_env_new(SERD_EMPTY_STRING());

  // Set a valid prefix
  assert(!serd_env_set_prefix(env, name1, eg));

  // Test setting a prefix from a relative URI
  assert(serd_env_set_prefix(env, name2, rel) == SERD_ERR_BAD_ARG);
  assert(!serd_env_set_base_uri(env, base));
  assert(!serd_env_set_prefix(env, name2, rel));

  // Test setting a prefix from strings
  assert(!serd_env_set_prefix(
    env, SERD_STRING("eg.3"), SERD_STRING("http://example.org/three")));

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
  SerdNode* const untyped = serd_new_string(SERD_STRING("data"));
  SerdEnv* const  env     = serd_env_new(SERD_EMPTY_STRING());

  assert(!serd_env_expand_node(env, untyped));

  serd_env_free(env);
  serd_node_free(untyped);
}

static void
test_expand_bad_uri_datatype(void)
{
  static const SerdStringView type = SERD_STRING("Type");

  SerdNodes* nodes = serd_nodes_new();

  const SerdNode* const typed =
    serd_nodes_literal(nodes, SERD_STRING("data"), SERD_HAS_DATATYPE, type);

  SerdEnv* const env = serd_env_new(SERD_EMPTY_STRING());

  assert(!serd_env_expand_node(env, typed));

  serd_env_free(env);
  serd_nodes_free(nodes);
}

static void
test_expand_uri(void)
{
  static const SerdStringView base = SERD_STRING("http://example.org/b/");

  SerdEnv* const  env       = serd_env_new(base);
  SerdNode* const rel       = serd_new_uri(SERD_STRING("rel"));
  SerdNode* const rel_out   = serd_env_expand_node(env, rel);
  SerdNode* const empty     = serd_new_uri(SERD_EMPTY_STRING());
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
  static const SerdStringView base = SERD_STRING("http://example.org/b/");

  SerdNode* const rel     = serd_new_uri(SERD_STRING("rel"));
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
  SerdNode* const bad_uri = serd_new_uri(SERD_STRING("rel"));
  SerdEnv* const  env     = serd_env_new(SERD_EMPTY_STRING());

  assert(!serd_env_expand_node(env, bad_uri));

  serd_env_free(env);
  serd_node_free(bad_uri);
}

static void
test_expand_curie(void)
{
  static const SerdStringView name = SERD_STRING("eg.1");
  static const SerdStringView eg   = SERD_STRING(NS_EG);

  SerdEnv* const env = serd_env_new(SERD_EMPTY_STRING());

  assert(!serd_env_set_prefix(env, name, eg));

  SerdNode* const expanded =
    serd_env_expand_curie(env, SERD_STRING("eg.1:foo"));

  assert(expanded);
  assert(!strcmp(serd_node_string(expanded), "http://example.org/foo"));
  serd_node_free(expanded);

  serd_env_free(env);
}

static void
test_expand_bad_curie(void)
{
  SerdEnv* const env = serd_env_new(SERD_EMPTY_STRING());

  assert(!serd_env_expand_curie(NULL, SERD_EMPTY_STRING()));
  assert(!serd_env_expand_curie(NULL, SERD_STRING("what:ever")));
  assert(!serd_env_expand_curie(env, SERD_STRING("eg.1:foo")));
  assert(!serd_env_expand_curie(env, SERD_STRING("nocolon")));

  serd_env_free(env);
}

static void
test_expand_blank(void)
{
  SerdNode* const blank = serd_new_token(SERD_BLANK, SERD_STRING("b1"));
  SerdEnv* const  env   = serd_env_new(SERD_EMPTY_STRING());

  assert(!serd_env_expand_node(env, blank));

  serd_env_free(env);
  serd_node_free(blank);
}

static void
test_equals(void)
{
  static const SerdStringView name1 = SERD_STRING("n1");
  static const SerdStringView base1 = SERD_STRING(NS_EG "b1/");
  static const SerdStringView base2 = SERD_STRING(NS_EG "b2/");

  SerdEnv* const env1 = serd_env_new(base1);
  SerdEnv* const env2 = serd_env_new(base2);

  assert(!serd_env_equals(env1, NULL));
  assert(!serd_env_equals(NULL, env1));
  assert(serd_env_equals(NULL, NULL));
  assert(!serd_env_equals(env1, env2));

  serd_env_set_base_uri(env2, base1);
  assert(serd_env_equals(env1, env2));

  assert(!serd_env_set_prefix(env1, name1, SERD_STRING(NS_EG "n1")));
  assert(!serd_env_equals(env1, env2));
  assert(!serd_env_set_prefix(env2, name1, SERD_STRING(NS_EG "othern1")));
  assert(!serd_env_equals(env1, env2));
  assert(!serd_env_set_prefix(env2, name1, SERD_STRING(NS_EG "n1")));
  assert(serd_env_equals(env1, env2));

  serd_env_set_base_uri(env2, base2);
  assert(!serd_env_equals(env1, env2));

  SerdEnv* const env3 = serd_env_copy(env2);
  assert(serd_env_equals(env3, env2));
  serd_env_free(env3);

  serd_env_free(env1);
  serd_env_free(env2);
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
