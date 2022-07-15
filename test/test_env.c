// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/string_view.h"

#include <assert.h>
#include <string.h>

static SerdStatus
count_prefixes(void* handle, const SerdEvent* event)
{
  if (event->type == SERD_PREFIX) {
    ++*(int*)handle;
  }

  return SERD_SUCCESS;
}

static void
test_env(void)
{
  const SerdStringView prefix = serd_string("eg.2");
  const SerdStringView eg     = serd_string("http://example.org/");

  SerdNode* hello = serd_new_string(serd_string("hello\""));
  SerdNode* rel   = serd_new_uri(serd_string("rel"));
  SerdNode* foo_u = serd_new_uri(serd_string("http://example.org/foo"));
  SerdNode* foo_c = serd_new_curie(serd_string("eg.2:foo"));
  SerdNode* b     = serd_new_curie(serd_string("invalid"));

  SerdEnv* env = serd_env_new(serd_empty_string());

  serd_env_set_prefix(env, prefix, eg);

  assert(!serd_env_base_uri(NULL));
  assert(!serd_env_expand(NULL, NULL));
  assert(!serd_env_expand(NULL, foo_c));
  assert(!serd_env_expand(env, NULL));
  assert(!serd_env_qualify(NULL, NULL));
  assert(!serd_env_qualify(env, NULL));
  assert(!serd_env_qualify(NULL, foo_u));

  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, serd_empty_string()));
  assert(!serd_env_base_uri(env));

  serd_env_set_prefix(
    env, serd_string("eg.2"), serd_string("http://example.org/"));

  assert(serd_env_set_prefix(env, serd_string("eg.3"), serd_string("rel")) ==
         SERD_ERR_BAD_ARG);

  SerdNode* xnode = serd_env_expand(env, hello);
  assert(!xnode);

  assert(!serd_env_expand(env, b));
  assert(!serd_env_expand(env, hello));

  assert(!serd_env_set_base_uri(env, serd_empty_string()));

  SerdNode* xu = serd_env_expand(env, foo_c);
  assert(!strcmp(serd_node_string(xu), "http://example.org/foo"));
  serd_node_free(xu);

  SerdNode* badpre  = serd_new_curie(serd_string("hm:what"));
  SerdNode* xbadpre = serd_env_expand(env, badpre);
  assert(!xbadpre);
  serd_node_free(badpre);

  SerdNode* xc = serd_env_expand(env, foo_c);
  assert(serd_node_equals(xc, foo_u));
  serd_node_free(xc);

  SerdNode* blank = serd_new_blank(serd_string("b1"));
  assert(!serd_env_expand(env, blank));
  serd_node_free(blank);

  size_t          n_prefixes = 0;
  SerdSink* const count_prefixes_sink =
    serd_sink_new(&n_prefixes, count_prefixes, NULL);

  serd_env_set_prefix(env, prefix, eg);
  serd_env_write_prefixes(env, count_prefixes_sink);
  assert(n_prefixes == 1);

  SerdNode* qualified = serd_env_qualify(env, foo_u);
  assert(serd_node_equals(qualified, foo_c));
  serd_node_free(qualified);

  SerdNode* unqualifiable = serd_new_uri(serd_string("http://drobilla.net/"));
  assert(!serd_env_qualify(env, unqualifiable));
  serd_node_free(unqualifiable);

  assert(!serd_env_expand(env, rel));
  serd_env_set_base_uri(env, serd_string("http://example.org/base/"));

  SerdNode* xrel = serd_env_expand(env, rel);
  assert(xrel);
  assert(!strcmp(serd_node_string(xrel), "http://example.org/base/rel"));
  serd_node_free(xrel);

  serd_sink_free(count_prefixes_sink);
  serd_node_free(b);
  serd_node_free(foo_c);
  serd_node_free(foo_u);
  serd_node_free(rel);
  serd_node_free(hello);
  serd_env_free(env);
}

int
main(void)
{
  test_env();
  return 0;
}
