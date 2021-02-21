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
  SerdNodes* nodes = serd_nodes_new();

  const SerdNode* u =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/foo"));

  const SerdNode* b = serd_nodes_curie(nodes, SERD_STRING("invalid"));
  const SerdNode* e = serd_nodes_uri(nodes, SERD_EMPTY_STRING());
  const SerdNode* c = serd_nodes_curie(nodes, SERD_STRING("eg.2:b"));
  const SerdNode* s = serd_nodes_string(nodes, SERD_STRING("hello"));

  SerdEnv* env = serd_env_new(SERD_EMPTY_STRING());

  const SerdNode* prefix_node = NULL;
  SerdStringView  prefix      = SERD_EMPTY_STRING();
  SerdStringView  suffix      = SERD_EMPTY_STRING();

  assert(!serd_env_qualify(NULL, u, &prefix_node, &suffix));

  assert(serd_env_expand(env, NULL, &prefix, &suffix) == SERD_ERR_BAD_CURIE);

  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));
  assert(!serd_env_base_uri(env));
  assert(!serd_env_base_uri(env));

  serd_env_set_prefix(
    env, SERD_STRING("eg.2"), SERD_STRING("http://example.org/"));

  assert(serd_env_set_prefix(env, SERD_STRING("eg.3"), SERD_STRING("rel")) ==
         SERD_ERR_BAD_ARG);

  assert(!serd_env_expand_node(NULL, u));
  assert(!serd_env_expand_node(env, b));
  assert(!serd_env_expand_node(env, s));
  assert(!serd_env_expand_node(env, e));

  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));

  SerdNode* xu = serd_env_expand_node(env, u);
  assert(!strcmp(serd_node_string(xu), "http://example.org/foo"));
  serd_node_free(xu);

  const SerdNode* badpre  = serd_nodes_curie(nodes, SERD_STRING("hm:what"));
  SerdNode*       xbadpre = serd_env_expand_node(env, badpre);
  assert(!xbadpre);

  SerdNode* xc = serd_env_expand_node(env, c);
  assert(!strcmp(serd_node_string(xc), "http://example.org/b"));
  serd_node_free(xc);

  const SerdNode* blank = serd_nodes_blank(nodes, SERD_STRING("b1"));
  assert(!serd_env_expand_node(env, blank));

  size_t          n_prefixes = 0;
  SerdSink* const count_prefixes_sink =
    serd_sink_new(&n_prefixes, count_prefixes, NULL);

  serd_env_set_prefix(
    env, SERD_STRING("eg.2"), SERD_STRING("http://example.org/"));
  serd_env_write_prefixes(env, count_prefixes_sink);
  assert(n_prefixes == 1);

  const SerdNode* shorter_uri = serd_nodes_uri(nodes, SERD_STRING("urn:foo"));
  assert(!serd_env_qualify(env, shorter_uri, &prefix_node, &suffix));

  assert(!serd_env_set_base_uri(env, serd_node_string_view(u)));
  assert(serd_node_equals(serd_env_base_uri(env), u));

  SerdNode* xe = serd_env_expand_node(env, e);
  assert(xe);
  assert(!strcmp(serd_node_string(xe), "http://example.org/foo"));
  serd_node_free(xe);

  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));
  assert(!serd_env_base_uri(env));

  serd_sink_free(count_prefixes_sink);
  serd_nodes_free(nodes);
  serd_env_free(env);
}

int
main(void)
{
  test_env();
  return 0;
}
