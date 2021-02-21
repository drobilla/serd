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
  SerdNode* u   = serd_new_uri(serd_string("http://example.org/foo"));
  SerdNode* b   = serd_new_curie(serd_string("invalid"));
  SerdNode* e   = serd_new_uri(serd_empty_string());
  SerdNode* c   = serd_new_curie(serd_string("eg.2:b"));
  SerdNode* s   = serd_new_string(serd_string("hello"));
  SerdEnv*  env = serd_env_new(serd_empty_string());

  const SerdNode* prefix_node = NULL;
  SerdStringView  prefix      = serd_empty_string();
  SerdStringView  suffix      = serd_empty_string();

  assert(!serd_env_qualify(NULL, u, &prefix_node, &suffix));

  assert(serd_env_expand(env, NULL, &prefix, &suffix) == SERD_ERR_BAD_CURIE);

  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, serd_empty_string()));
  assert(!serd_env_base_uri(env));
  assert(!serd_env_base_uri(env));

  serd_env_set_prefix(
    env, serd_string("eg.2"), serd_string("http://example.org/"));

  assert(serd_env_set_prefix(env, serd_string("eg.3"), serd_string("rel")) ==
         SERD_ERR_BAD_ARG);

  assert(!serd_env_expand_node(NULL, u));
  assert(!serd_env_expand_node(env, b));
  assert(!serd_env_expand_node(env, s));
  assert(!serd_env_expand_node(env, e));

  assert(!serd_env_set_base_uri(env, serd_empty_string()));

  SerdNode* xu = serd_env_expand_node(env, u);
  assert(!strcmp(serd_node_string(xu), "http://example.org/foo"));
  serd_node_free(xu);

  SerdNode* badpre  = serd_new_curie(serd_string("hm:what"));
  SerdNode* xbadpre = serd_env_expand_node(env, badpre);
  assert(!xbadpre);

  SerdNode* xc = serd_env_expand_node(env, c);
  assert(!strcmp(serd_node_string(xc), "http://example.org/b"));
  serd_node_free(xc);

  SerdNode* blank = serd_new_blank(serd_string("b1"));
  assert(!serd_env_expand_node(env, blank));
  serd_node_free(blank);

  size_t          n_prefixes = 0;
  SerdSink* const count_prefixes_sink =
    serd_sink_new(&n_prefixes, count_prefixes, NULL);

  serd_env_set_prefix(
    env, serd_string("eg.2"), serd_string("http://example.org/"));
  serd_env_write_prefixes(env, count_prefixes_sink);
  assert(n_prefixes == 1);

  SerdNode* shorter_uri = serd_new_uri(serd_string("urn:foo"));
  assert(!serd_env_qualify(env, shorter_uri, &prefix_node, &suffix));

  assert(!serd_env_set_base_uri(env, serd_node_string_view(u)));
  assert(serd_node_equals(serd_env_base_uri(env), u));

  SerdNode* xe = serd_env_expand_node(env, e);
  assert(xe);
  assert(!strcmp(serd_node_string(xe), "http://example.org/foo"));
  serd_node_free(xe);

  assert(!serd_env_set_base_uri(env, serd_empty_string()));
  assert(!serd_env_base_uri(env));

  serd_sink_free(count_prefixes_sink);
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
