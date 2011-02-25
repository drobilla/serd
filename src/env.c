/*
  Copyright 2011 David Robillard <http://drobilla.net>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
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
