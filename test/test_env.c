/*
  Copyright 2011-2020 David Robillard <http://drobilla.net>

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
#include <stdint.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

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
	SerdNode u   = serd_node_from_string(SERD_URI, USTR("http://example.org/foo"));
	SerdNode b   = serd_node_from_string(SERD_CURIE, USTR("invalid"));
	SerdNode c   = serd_node_from_string(SERD_CURIE, USTR("eg.2:b"));
	SerdEnv* env = serd_env_new(NULL);
	serd_env_set_prefix_from_strings(env, USTR("eg.2"), USTR("http://example.org/"));

	assert(!serd_env_set_base_uri(env, NULL));
	assert(serd_env_set_base_uri(env, &SERD_NODE_NULL));
	assert(serd_node_equals(serd_env_get_base_uri(env, NULL), &SERD_NODE_NULL));

	SerdChunk prefix;
	SerdChunk suffix;
	assert(serd_env_expand(env, &b, &prefix, &suffix));

	SerdNode xnode = serd_env_expand_node(env, &SERD_NODE_NULL);
	assert(serd_node_equals(&xnode, &SERD_NODE_NULL));

	SerdNode xu = serd_env_expand_node(env, &u);
	assert(!strcmp((const char*)xu.buf, "http://example.org/foo"));
	serd_node_free(&xu);

	SerdNode badpre = serd_node_from_string(SERD_CURIE, USTR("hm:what"));
	SerdNode xbadpre = serd_env_expand_node(env, &badpre);
	assert(serd_node_equals(&xbadpre, &SERD_NODE_NULL));

	SerdNode xc = serd_env_expand_node(env, &c);
	assert(!strcmp((const char*)xc.buf, "http://example.org/b"));
	serd_node_free(&xc);

	assert(serd_env_set_prefix(env, &SERD_NODE_NULL, &SERD_NODE_NULL));

	const SerdNode lit = serd_node_from_string(SERD_LITERAL, USTR("hello"));
	assert(serd_env_set_prefix(env, &b, &lit));

	const SerdNode blank  = serd_node_from_string(SERD_BLANK, USTR("b1"));
	const SerdNode xblank = serd_env_expand_node(env, &blank);
	assert(serd_node_equals(&xblank, &SERD_NODE_NULL));

	int n_prefixes = 0;
	serd_env_set_prefix_from_strings(env, USTR("eg.2"), USTR("http://example.org/"));
	serd_env_foreach(env, count_prefixes, &n_prefixes);
	assert(n_prefixes == 1);

	SerdNode shorter_uri = serd_node_from_string(SERD_URI, USTR("urn:foo"));
	SerdNode prefix_name;
	assert(!serd_env_qualify(env, &shorter_uri, &prefix_name, &suffix));

	assert(!serd_env_set_base_uri(env, &u));
	assert(serd_node_equals(serd_env_get_base_uri(env, NULL), &u));
	assert(!serd_env_set_base_uri(env, NULL));
	assert(!serd_env_get_base_uri(env, NULL)->buf);

	serd_env_free(env);
}

int
main(void)
{
	test_env();
	return 0;
}
