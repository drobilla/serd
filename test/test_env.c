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

static SerdStatus
count_prefixes(void* handle, const SerdEvent* event)
{
  *(int*)handle += event->type == SERD_PREFIX;

  return SERD_SUCCESS;
}

static void
test_null(void)
{
  SerdNode* const eg = serd_new_uri(SERD_STATIC_STRING(NS_EG));

  // "Copying" NULL returns null
  assert(!serd_env_copy(NULL));

  // Accessors are tolerant to a NULL env for convenience
  assert(!serd_env_base_uri(NULL));
  assert(!serd_env_expand(NULL, NULL));
  assert(!serd_env_qualify(NULL, eg));

  // Only null is equal to null
  assert(serd_env_equals(NULL, NULL));

  serd_node_free(eg);
}

static void
test_base_uri(void)
{
  SerdEnv* const  env   = serd_env_new(SERD_EMPTY_STRING());
  SerdNode* const empty = serd_new_uri(SERD_STATIC_STRING(""));
  SerdNode* const hello = serd_new_string(SERD_STATIC_STRING("hello"));
  SerdNode* const eg    = serd_new_uri(SERD_STATIC_STRING(NS_EG));

  // Test that invalid calls work as expected
  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));
  assert(!serd_env_base_uri(env));

  // Try setting a relative prefix with no base URI
  assert(serd_env_set_prefix(env,
                             SERD_STATIC_STRING("eg.3"),
                             SERD_STATIC_STRING("rel")) == SERD_ERR_BAD_ARG);

  // Set a valid base URI
  assert(!serd_env_set_base_uri(env, serd_node_string_view(eg)));
  assert(serd_node_equals(serd_env_base_uri(env), eg));

  // Reset the base URI
  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));
  assert(!serd_env_base_uri(env));

  serd_node_free(eg);
  serd_node_free(hello);
  serd_node_free(empty);
  serd_env_free(env);
}

static void
test_set_prefix(void)
{
  static const SerdStringView eg    = SERD_STATIC_STRING(NS_EG);
  static const SerdStringView name1 = SERD_STATIC_STRING("eg.1");
  static const SerdStringView name2 = SERD_STATIC_STRING("eg.2");
  static const SerdStringView rel   = SERD_STATIC_STRING("rel");
  static const SerdStringView base  = SERD_STATIC_STRING("http://example.org/");

  SerdEnv* const env = serd_env_new(SERD_EMPTY_STRING());

  // Set a valid prefix
  assert(!serd_env_set_prefix(env, name1, eg));

  // Test setting a prefix from a relative URI
  assert(serd_env_set_prefix(env, name2, rel) == SERD_ERR_BAD_ARG);
  assert(!serd_env_set_base_uri(env, base));
  assert(!serd_env_set_prefix(env, name2, rel));

  // Test setting a prefix from strings
  assert(!serd_env_set_prefix(env,
                              SERD_STATIC_STRING("eg.3"),
                              SERD_STATIC_STRING("http://example.org/three")));

  size_t          n_prefixes = 0;
  SerdSink* const count_prefixes_sink =
    serd_sink_new(&n_prefixes, count_prefixes, NULL);

  serd_env_write_prefixes(env, count_prefixes_sink);
  serd_sink_free(count_prefixes_sink);
  assert(n_prefixes == 3);

  serd_env_free(env);
}

static void
test_expand(void)
{
  static const SerdStringView eg   = SERD_STATIC_STRING(NS_EG);
  static const SerdStringView type = SERD_STATIC_STRING("Type");
  static const SerdStringView base =
    SERD_STATIC_STRING("http://example.org/b/");

  SerdNode* const name  = serd_new_string(SERD_STATIC_STRING("eg.1"));
  SerdNode* const blank = serd_new_blank(SERD_STATIC_STRING("b1"));
  SerdNode* const rel   = serd_new_uri(SERD_STATIC_STRING("rel"));
  SerdNode* const c1    = serd_new_curie(SERD_STATIC_STRING("eg.1:foo"));
  SerdNode* const c1_full =
    serd_new_uri(SERD_STATIC_STRING("http://example.org/foo"));
  SerdNode* const c2 = serd_new_curie(SERD_STATIC_STRING("hm:what"));
  SerdNode* const typed =
    serd_new_typed_literal(SERD_STATIC_STRING("data"), type);
  SerdEnv* const env = serd_env_new(base);

  assert(!serd_env_set_prefix(env, serd_node_string_view(name), eg));

  assert(!serd_env_expand(env, name));
  assert(!serd_env_expand(env, blank));

  // Expand CURIE
  SerdNode* const c1_out = serd_env_expand(env, c1);
  assert(serd_node_equals(c1_out, c1_full));
  serd_node_free(c1_out);

  // Expand relative URI
  SerdNode* const rel_out = serd_env_expand(env, rel);
  assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
  serd_node_free(rel_out);

  // Expand literal with URI datatype
  SerdNode* const typed_out = serd_env_expand(env, typed);
  assert(typed_out);
  assert(!strcmp(serd_node_string(typed_out), "data"));

  const SerdNode* const datatype = serd_node_datatype(typed_out);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), "http://example.org/b/Type"));
  serd_node_free(typed_out);

  assert(!serd_env_expand(env, c2));

  serd_env_free(env);
  serd_node_free(typed);
  serd_node_free(c2);
  serd_node_free(c1_full);
  serd_node_free(c1);
  serd_node_free(rel);
  serd_node_free(blank);
  serd_node_free(name);
}

static void
test_qualify(void)
{
  static const SerdStringView eg = SERD_STATIC_STRING(NS_EG);

  SerdNode* const name = serd_new_string(SERD_STATIC_STRING("eg"));
  SerdNode* const c1   = serd_new_curie(SERD_STATIC_STRING("eg:foo"));

  SerdNode* const u1 =
    serd_new_uri(SERD_STATIC_STRING("http://example.org/foo"));

  SerdNode* const u2 =
    serd_new_uri(SERD_STATIC_STRING("http://drobilla.net/bar"));

  SerdEnv* const env = serd_env_new(SERD_EMPTY_STRING());

  assert(!serd_env_set_prefix(env, serd_node_string_view(name), eg));

  assert(!serd_env_expand(env, name));

  SerdNode* const u1_out = serd_env_qualify(env, u1);
  assert(serd_node_equals(u1_out, c1));
  serd_node_free(u1_out);

  assert(!serd_env_qualify(env, u2));

  serd_env_free(env);
  serd_node_free(u2);
  serd_node_free(c1);
  serd_node_free(u1);
  serd_node_free(name);
}

static void
test_equals(void)
{
  static const SerdStringView name1 = SERD_STATIC_STRING("n1");
  static const SerdStringView base1 = SERD_STATIC_STRING(NS_EG "b1/");
  static const SerdStringView base2 = SERD_STATIC_STRING(NS_EG "b2/");

  SerdEnv* const env1 = serd_env_new(base1);
  SerdEnv* const env2 = serd_env_new(base2);

  assert(!serd_env_equals(env1, NULL));
  assert(!serd_env_equals(NULL, env1));
  assert(serd_env_equals(NULL, NULL));
  assert(!serd_env_equals(env1, env2));

  serd_env_set_base_uri(env2, base1);
  assert(serd_env_equals(env1, env2));

  assert(!serd_env_set_prefix(env1, name1, SERD_STATIC_STRING(NS_EG "n1")));
  assert(!serd_env_equals(env1, env2));
  assert(
    !serd_env_set_prefix(env2, name1, SERD_STATIC_STRING(NS_EG "othern1")));
  assert(!serd_env_equals(env1, env2));
  assert(!serd_env_set_prefix(env2, name1, SERD_STATIC_STRING(NS_EG "n1")));
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
  test_null();
  test_base_uri();
  test_set_prefix();
  test_expand();
  test_qualify();
  test_equals();
  return 0;
}
