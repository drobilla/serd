/* Serd, a lightweight RDF syntax library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 *
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file env.c */

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
};

SERD_API
SerdEnv
serd_env_new()
{
	SerdEnv env = malloc(sizeof(struct SerdEnvImpl));
	env->prefixes   = NULL;
	env->n_prefixes = 0;
	return env;
}

SERD_API
void
serd_env_free(SerdEnv env)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		serd_node_free(&env->prefixes[i].name);
		serd_node_free(&env->prefixes[i].uri);
	}
	free(env->prefixes);
	free(env);
}

static inline SerdPrefix*
serd_env_find(SerdEnv        env,
              const uint8_t* name,
              size_t         name_len)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		const SerdNode* const prefix_name = &env->prefixes[i].name;
		if (prefix_name->n_bytes == name_len + 1) {
			if (!memcmp(prefix_name->buf, name, name_len)) {
				return &env->prefixes[i];
			}
		}
	}
	return NULL;
}

SERD_API
void
serd_env_add(SerdEnv         env,
             const SerdNode* name,
             const SerdNode* uri)
{
	assert(name && uri);
	SerdPrefix* const prefix = serd_env_find(env, name->buf, name->n_chars);
	if (prefix) {
		serd_node_free(&prefix->uri);
		prefix->uri = serd_node_copy(uri);
	} else {
		env->prefixes = realloc(env->prefixes,
		                        (++env->n_prefixes) * sizeof(SerdPrefix));
		env->prefixes[env->n_prefixes - 1].name = serd_node_copy(name);
		env->prefixes[env->n_prefixes - 1].uri  = serd_node_copy(uri);
	}
}

SERD_API
bool
serd_env_qualify(const SerdEnv   env,
                 const SerdNode* uri,
                 SerdNode*       prefix_name,
                 SerdChunk*      suffix)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		const SerdNode* const prefix_uri = &env->prefixes[i].uri;
		if (uri->n_bytes >= prefix_uri->n_bytes) {
			if (!strncmp((const char*)uri->buf,
			             (const char*)prefix_uri->buf,
			             prefix_uri->n_bytes - 1)) {
				*prefix_name = env->prefixes[i].name;
				suffix->buf = uri->buf + prefix_uri->n_bytes - 1;
				suffix->len = uri->n_bytes - prefix_uri->n_bytes;
				return true;
			}
		}
	}
	return false;
}

SERD_API
bool
serd_env_expand(const SerdEnv   env,
                const SerdNode* qname,
                SerdChunk*      uri_prefix,
                SerdChunk*      uri_suffix)
{
	const uint8_t* const colon = memchr(qname->buf, ':', qname->n_bytes);
	if (!colon) {
		return false;  // Illegal qname
	}

	const size_t            name_len = colon - qname->buf;
	const SerdPrefix* const prefix   = serd_env_find(env, qname->buf, name_len);
	if (prefix) {
		uri_prefix->buf = prefix->uri.buf;
		uri_prefix->len = prefix->uri.n_bytes - 1;
		uri_suffix->buf = colon + 1;
		uri_suffix->len = qname->n_bytes - (colon - qname->buf) - 2;
		return true;
	}
	return false;
}

SERD_API
void
serd_env_foreach(const SerdEnv  env,
                 SerdPrefixSink func,
                 void*          handle)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		func(handle,
		     &env->prefixes[i].name,
		     &env->prefixes[i].uri);
	}
}
