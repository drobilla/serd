// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/node.h"
#include "serd/status.h"
#include "zix/string_view.h"

#include <assert.h>
#include <string.h>

#define NS_EG "http://example.org/"

static SerdStatus
count_prefixes(void* handle, const SerdNode* name, const SerdNode* uri)
{
  (void)name;
  (void)uri;

  ++*(int*)handle;
  return SERD_SUCCESS;
}

static void
test_env(void)
{
  SerdNode* u   = serd_new_uri(zix_string(NS_EG "foo"));
  SerdNode* b   = serd_new_curie(zix_string("invalid"));
  SerdNode* e   = serd_new_uri(zix_empty_string());
  SerdNode* c   = serd_new_curie(zix_string("eg.2:b"));
  SerdNode* s   = serd_new_string(zix_string("hello"));
  SerdEnv*  env = serd_env_new(zix_empty_string());

  const SerdNode* prefix_node = NULL;
  ZixStringView   prefix      = zix_empty_string();
  ZixStringView   suffix      = zix_empty_string();

  assert(!serd_env_qualify(NULL, u, &prefix_node, &suffix));

  assert(serd_env_expand(env, NULL, &prefix, &suffix) == SERD_BAD_CURIE);

  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_env_base_uri(env));
  assert(!serd_env_base_uri(env));

  serd_env_set_prefix(env, zix_string("eg.2"), zix_string(NS_EG));

  assert(serd_env_set_prefix(env, zix_string("eg.3"), zix_string("rel")) ==
         SERD_BAD_ARG);

  assert(!serd_env_expand_node(NULL, u));
  assert(!serd_env_expand_node(env, b));
  assert(!serd_env_expand_node(env, s));
  assert(!serd_env_expand_node(env, e));

  assert(!serd_env_set_base_uri(env, zix_empty_string()));

  SerdNode* xu = serd_env_expand_node(env, u);
  assert(!strcmp(serd_node_string(xu), NS_EG "foo"));
  serd_node_free(xu);

  SerdNode* badpre  = serd_new_curie(zix_string("hm:what"));
  SerdNode* xbadpre = serd_env_expand_node(env, badpre);
  assert(!xbadpre);

  SerdNode* xc = serd_env_expand_node(env, c);
  assert(!strcmp(serd_node_string(xc), NS_EG "b"));
  serd_node_free(xc);

  SerdNode* blank = serd_new_blank(zix_string("b1"));
  assert(!serd_env_expand_node(env, blank));
  serd_node_free(blank);

  int n_prefixes = 0;
  serd_env_set_prefix(env, zix_string("eg.2"), zix_string(NS_EG));
  serd_env_foreach(env, count_prefixes, &n_prefixes);
  assert(n_prefixes == 1);

  SerdNode* shorter_uri = serd_new_uri(zix_string("urn:foo"));
  assert(!serd_env_qualify(env, shorter_uri, &prefix_node, &suffix));

  assert(!serd_env_set_base_uri(env, serd_node_string_view(u)));
  assert(serd_node_equals(serd_env_base_uri(env), u));

  SerdNode* xe = serd_env_expand_node(env, e);
  assert(xe);
  assert(!strcmp(serd_node_string(xe), NS_EG "foo"));
  serd_node_free(xe);

  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_env_base_uri(env));

  serd_node_free(shorter_uri);
  serd_node_free(badpre);
  serd_node_free(s);
  serd_node_free(c);
  serd_node_free(e);
  serd_node_free(b);
  serd_node_free(u);

  serd_env_free(env);
}

int
main(void)
{
  test_env();
  return 0;
}
