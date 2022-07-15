// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "zix/string_view.h"

#include <assert.h>
#include <string.h>

#define NS_EG "http://example.org/"

static void
test_copy(void)
{
  assert(!serd_env_copy(NULL));

  SerdEnv* const env = serd_env_new(zix_string(NS_EG "base/"));

  serd_env_set_prefix(env, zix_string("eg"), zix_string(NS_EG));

  SerdEnv* const env_copy = serd_env_copy(env);
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

  SerdEnv* const env1 = serd_env_new(base1);
  SerdEnv* const env2 = serd_env_new(base2);

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

  SerdEnv* const env3 = serd_env_copy(env2);
  assert(serd_env_equals(env3, env2));
  serd_env_free(env3);

  serd_env_free(env2);
  serd_env_free(env1);
}

static void
test_null(void)
{
  // "Copying" NULL returns null
  assert(!serd_env_copy(NULL));

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
  assert(!serd_env_new(zix_string("rel")));

  SerdEnv* const  env = serd_env_new(zix_empty_string());
  SerdNode* const eg  = serd_new_uri(zix_string(NS_EG));

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
  serd_node_free(eg);
}

static void
test_set_prefix(void)
{
  static const ZixStringView eg    = ZIX_STATIC_STRING(NS_EG);
  static const ZixStringView name1 = ZIX_STATIC_STRING("eg.1");
  static const ZixStringView name2 = ZIX_STATIC_STRING("eg.2");
  static const ZixStringView rel   = ZIX_STATIC_STRING("rel");
  static const ZixStringView base  = ZIX_STATIC_STRING(NS_EG);

  SerdEnv* const env = serd_env_new(zix_empty_string());

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
    serd_sink_new(&n_prefixes, count_prefixes, NULL);

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

  SerdEnv* const env = serd_env_new(zix_empty_string());

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

  SerdEnv* const env = serd_env_new(zix_empty_string());

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

  SerdNode* const name = serd_new_string(zix_string("eg"));
  SerdNode* const c1   = serd_new_curie(zix_string("eg:foo"));
  SerdNode* const u1   = serd_new_uri(zix_string(NS_EG "foo"));
  SerdNode* const u2   = serd_new_uri(zix_string("http://drobilla.net/bar"));

  SerdEnv* const env = serd_env_new(zix_empty_string());

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
  serd_node_free(u2);
  serd_node_free(u1);
  serd_node_free(c1);
  serd_node_free(name);
}

static void
test_sink(void)
{
  SerdNode* const base = serd_new_uri(zix_string(NS_EG));
  SerdNode* const name = serd_new_string(zix_string("eg"));
  SerdNode* const uri  = serd_new_uri(zix_string(NS_EG "uri"));
  SerdEnv* const  env  = serd_env_new(zix_empty_string());

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
  serd_node_free(uri);
  serd_node_free(name);
  serd_node_free(base);
}

int
main(void)
{
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
