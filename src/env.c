/*
  Copyright 2011 David Robillard <http://drobilla.net>

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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "serd_internal.h"

typedef struct {
	SerdNode name;
	SerdNode uri;
} SerdPrefix;

struct SerdEnvImpl {
	SerdPrefix* prefixes;
	size_t      n_prefixes;
	SerdNode    base_uri_node;
	SerdURI     base_uri;
};

SERD_API
SerdEnv*
serd_env_new(const SerdNode* base_uri)
{
	SerdEnv* env = malloc(sizeof(struct SerdEnvImpl));
	env->prefixes      = NULL;
	env->n_prefixes    = 0;
	env->base_uri_node = SERD_NODE_NULL;
	env->base_uri      = SERD_URI_NULL;
	if (base_uri) {
		serd_env_set_base_uri(env, base_uri);
	}
	return env;
}

SERD_API
void
serd_env_free(SerdEnv* env)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		serd_node_free(&env->prefixes[i].name);
		serd_node_free(&env->prefixes[i].uri);
	}
	free(env->prefixes);
	serd_node_free(&env->base_uri_node);
	free(env);
}

SERD_API
const SerdNode*
serd_env_get_base_uri(const SerdEnv* env,
                      SerdURI*       out)
{
	*out = env->base_uri;
	return &env->base_uri_node;
}

SERD_API
SerdStatus
serd_env_set_base_uri(SerdEnv*        env,
                      const SerdNode* uri_node)
{
	// Resolve base URI and create a new node and URI for it
	SerdURI  base_uri;
	SerdNode base_uri_node = serd_node_new_uri_from_node(
		uri_node, &env->base_uri, &base_uri);

	if (base_uri_node.buf) {
		// Replace the current base URI
		serd_node_free(&env->base_uri_node);
		env->base_uri_node = base_uri_node;
		env->base_uri      = base_uri;
		return SERD_SUCCESS;
	}
	return SERD_ERR_BAD_ARG;
}

static inline SerdPrefix*
serd_env_find(const SerdEnv* env,
              const uint8_t* name,
              size_t         name_len)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		const SerdNode* const prefix_name = &env->prefixes[i].name;
		if (prefix_name->n_bytes == name_len) {
			if (!memcmp(prefix_name->buf, name, name_len)) {
				return &env->prefixes[i];
			}
		}
	}
	return NULL;
}

static void
serd_env_add(SerdEnv*        env,
             const SerdNode* name,
             const SerdNode* uri)
{
	assert(name && uri);
	assert(name->n_bytes == strlen((const char*)name->buf));
	SerdPrefix* const prefix = serd_env_find(env, name->buf, name->n_bytes);
	if (prefix) {
		SerdNode old_prefix_uri = prefix->uri;
		prefix->uri = serd_node_copy(uri);
		serd_node_free(&old_prefix_uri);
	} else {
		env->prefixes = realloc(env->prefixes,
		                        (++env->n_prefixes) * sizeof(SerdPrefix));
		env->prefixes[env->n_prefixes - 1].name = serd_node_copy(name);
		env->prefixes[env->n_prefixes - 1].uri  = serd_node_copy(uri);
	}
}

SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv*        env,
                    const SerdNode* name,
                    const SerdNode* uri_node)
{
	if (serd_uri_string_has_scheme(uri_node->buf)) {
		// Set prefix to absolute URI
		serd_env_add(env, name, uri_node);
	} else {
		// Resolve relative URI and create a new node and URI for it
		SerdURI  abs_uri;
		SerdNode abs_uri_node = serd_node_new_uri_from_node(
			uri_node, &env->base_uri, &abs_uri);

		if (!abs_uri_node.buf) {
			return SERD_ERR_BAD_ARG;
		}

		// Set prefix to resolved (absolute) URI
		serd_env_add(env, name, &abs_uri_node);
		serd_node_free(&abs_uri_node);
	}
	return SERD_SUCCESS;
}

static inline bool
is_nameStartChar(const uint8_t c)
{
	// TODO: not strictly correct (see reader.c read_nameStartChar)
	return (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z');
}

static inline bool
is_nameChar(const uint8_t c)
{
	// TODO: 0x300-0x036F | 0x203F-0x2040 (see reader.c read_nameChar)
	return is_nameStartChar(c) || c == '-'
		|| (c >= '0' && c <= '9') || c == 0xB7;
}

/**
   Return true iff @c buf is a valid Turtle name.
*/
static inline bool
is_name(const uint8_t* buf,
        size_t         len)
{
	if (!is_nameStartChar(buf[0])) {
		return false;
	}

	for (size_t i = 1; i < len; ++i) {
		if (!is_nameChar(buf[i])) {
			return false;
		}
	}

	return true;
}

SERD_API
bool
serd_env_qualify(const SerdEnv*  env,
                 const SerdNode* uri,
                 SerdNode*       prefix_name,
                 SerdChunk*      suffix)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		const SerdNode* const prefix_uri = &env->prefixes[i].uri;
		if (uri->n_bytes >= prefix_uri->n_bytes) {
			if (!strncmp((const char*)uri->buf,
			             (const char*)prefix_uri->buf,
			             prefix_uri->n_bytes)) {
				*prefix_name = env->prefixes[i].name;
				suffix->buf = uri->buf + prefix_uri->n_bytes;
				suffix->len = uri->n_bytes - prefix_uri->n_bytes;
				if (!is_name(suffix->buf, suffix->len)) {
					continue;
				}
				return true;
			}
		}
	}
	return false;
}

SERD_API
SerdStatus
serd_env_expand(const SerdEnv*  env,
                const SerdNode* qname,
                SerdChunk*      uri_prefix,
                SerdChunk*      uri_suffix)
{
	const uint8_t* const colon = memchr(qname->buf, ':', qname->n_bytes + 1);
	if (!colon) {
		return SERD_ERR_BAD_ARG;  // Illegal qname
	}

	const size_t            name_len = colon - qname->buf;
	const SerdPrefix* const prefix   = serd_env_find(env, qname->buf, name_len);
	if (prefix) {
		uri_prefix->buf = prefix->uri.buf;
		uri_prefix->len = prefix->uri.n_bytes;
		uri_suffix->buf = colon + 1;
		uri_suffix->len = qname->n_bytes - (colon - qname->buf) - 1;
		return SERD_SUCCESS;
	}
	return SERD_ERR_NOT_FOUND;
}

SERD_API
SerdNode
serd_env_expand_node(const SerdEnv*  env,
                     const SerdNode* node)
{
	if (node->type == SERD_CURIE) {
		SerdChunk prefix;
		SerdChunk suffix;
		serd_env_expand(env, node, &prefix, &suffix);
		SerdNode ret = { NULL,
		                 prefix.len + suffix.len + 1,
		                 prefix.len + suffix.len,  // FIXME: UTF-8
		                 0,
		                 SERD_URI };
		ret.buf = malloc(ret.n_bytes + 1);
		snprintf((char*)ret.buf, ret.n_bytes + 1, "%s%s", prefix.buf, suffix.buf);
		return ret;
	} else if (node->type == SERD_URI) {
		SerdURI ignored;
		return serd_node_new_uri_from_node(node, &env->base_uri, &ignored);
	} else {
		return SERD_NODE_NULL;
	}
}

SERD_API
void
serd_env_foreach(const SerdEnv* env,
                 SerdPrefixSink func,
                 void*          handle)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		func(handle,
		     &env->prefixes[i].name,
		     &env->prefixes[i].uri);
	}
}
