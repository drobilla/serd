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
  static const SerdStringView prefix = SERD_STATIC_STRING("eg.2");
  static const SerdStringView eg = SERD_STATIC_STRING("http://example.org/");

  SerdNode* hello = serd_new_string(SERD_STATIC_STRING("hello\""));
  SerdNode* foo_u = serd_new_uri(SERD_STATIC_STRING("http://example.org/foo"));
  SerdNode* empty = serd_new_uri(SERD_STATIC_STRING(""));
  SerdNode* foo_c = serd_new_curie(SERD_STATIC_STRING("eg.2:foo"));
  SerdNode* b     = serd_new_curie(SERD_STATIC_STRING("invalid"));
  SerdEnv*  env   = serd_env_new(SERD_EMPTY_STRING());

  serd_env_set_prefix(env, prefix, eg);

  assert(!serd_env_base_uri(NULL));
  assert(!serd_env_expand(NULL, NULL));
  assert(!serd_env_qualify(NULL, foo_u));

  assert(!serd_env_base_uri(env));
  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));
  assert(!serd_env_base_uri(env));

  assert(serd_env_set_prefix(env,
                             SERD_STATIC_STRING("eg.3"),
                             SERD_STATIC_STRING("rel")) == SERD_ERR_BAD_ARG);

  SerdNode* xnode = serd_env_expand(env, hello);
  assert(!xnode);

  assert(!serd_env_expand(env, b));
  assert(!serd_env_expand(env, hello));

  assert(!serd_env_set_base_uri(env, SERD_EMPTY_STRING()));

  serd_node_free(hello);

  SerdNode* xu = serd_env_expand(env, foo_c);
  assert(!strcmp(serd_node_string(xu), "http://example.org/foo"));
  serd_node_free(xu);

  SerdNode* badpre  = serd_new_curie(SERD_STATIC_STRING("hm:what"));
  SerdNode* xbadpre = serd_env_expand(env, badpre);
  assert(!xbadpre);
  serd_node_free(badpre);

  SerdNode* xc = serd_env_expand(env, foo_c);
  assert(serd_node_equals(xc, foo_u));
  serd_node_free(xc);

  SerdNode* blank = serd_new_blank(SERD_STATIC_STRING("b1"));
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

  SerdEnv* env_copy = serd_env_copy(env);
  assert(serd_env_equals(env, env_copy));
  assert(!serd_env_equals(env, NULL));
  assert(!serd_env_equals(NULL, env));
  assert(serd_env_equals(NULL, NULL));

  SerdNode* qualified2 = serd_env_expand(env_copy, foo_u);
  assert(serd_node_equals(qualified, foo_c));
  serd_node_free(qualified2);

  serd_env_set_prefix(env_copy,
                      SERD_STATIC_STRING("test"),
                      SERD_STATIC_STRING("http://example.org/test"));

  assert(!serd_env_equals(env, env_copy));

  serd_env_set_prefix(env,
                      SERD_STATIC_STRING("test2"),
                      SERD_STATIC_STRING("http://example.org/test"));

  assert(!serd_env_equals(env, env_copy));

  serd_node_free(qualified);
  serd_sink_free(count_prefixes_sink);
  serd_node_free(foo_c);
  serd_node_free(empty);
  serd_node_free(foo_u);
  serd_node_free(b);
  serd_env_free(env_copy);

  serd_env_free(env);
}

int
main(void)
{
  test_env();
  return 0;
}
