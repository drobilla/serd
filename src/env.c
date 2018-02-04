// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "env.h"
#include "node.h"

#include "serd/env.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  SerdNode* name;
  SerdNode* uri;
} SerdPrefix;

struct SerdEnvImpl {
  SerdPrefix* prefixes;
  size_t      n_prefixes;
  SerdNode*   base_uri_node;
  SerdURIView base_uri;
};

SerdEnv*
serd_env_new(const SerdNode* const base_uri)
{
  SerdEnv* env = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));
  if (env && base_uri) {
    if (serd_env_set_base_uri(env, base_uri)) {
      free(env);
      return NULL;
    }
  }

  return env;
}

void
serd_env_free(SerdEnv* const env)
{
  if (!env) {
    return;
  }

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    serd_node_free(env->prefixes[i].name);
    serd_node_free(env->prefixes[i].uri);
  }
  free(env->prefixes);
  serd_node_free(env->base_uri_node);
  free(env);
}

const SerdNode*
serd_env_base_uri(const SerdEnv* const env, SerdURIView* const out)
{
  assert(env);

  if (out) {
    *out = env->base_uri;
  }

  return env->base_uri_node;
}

SerdStatus
serd_env_set_base_uri(SerdEnv* const env, const SerdNode* const uri)
{
  assert(env);

  if (uri && serd_node_type(uri) != SERD_URI) {
    return SERD_BAD_ARG;
  }

  if (!uri) {
    serd_node_free(env->base_uri_node);
    env->base_uri_node = NULL;
    env->base_uri      = SERD_URI_NULL;
    return SERD_SUCCESS;
  }

  // Resolve the new base against the current base in case it is relative
  const SerdURIView new_base_uri =
    serd_resolve_uri(serd_parse_uri(serd_node_string(uri)), env->base_uri);

  SerdNode* const new_base_node = serd_new_parsed_uri(new_base_uri);

  // Replace the current base URI
  serd_node_free(env->base_uri_node);
  env->base_uri_node = new_base_node;
  env->base_uri      = serd_node_uri_view(env->base_uri_node);

  return SERD_SUCCESS;
}

ZIX_PURE_FUNC static SerdPrefix*
serd_env_find(const SerdEnv* const env,
              const char* const    name,
              const size_t         name_len)
{
  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_name = env->prefixes[i].name;
    if (serd_node_length(prefix_name) == name_len) {
      if (!memcmp(serd_node_string(prefix_name), name, name_len)) {
        return &env->prefixes[i];
      }
    }
  }

  return NULL;
}

static void
serd_env_add(SerdEnv* const        env,
             const SerdNode* const name,
             const SerdNode* const uri)
{
  const char*       name_str = serd_node_string(name);
  const size_t      name_len = serd_node_length(name);
  SerdPrefix* const prefix   = serd_env_find(env, name_str, name_len);
  if (prefix) {
    if (!serd_node_equals(prefix->uri, uri)) {
      SerdNode* old_prefix_uri = prefix->uri;
      prefix->uri              = serd_node_copy(uri);
      serd_node_free(old_prefix_uri);
    }
  } else {
    SerdPrefix* const new_prefixes = (SerdPrefix*)realloc(
      env->prefixes, (++env->n_prefixes) * sizeof(SerdPrefix));
    if (new_prefixes) {
      env->prefixes                           = new_prefixes;
      env->prefixes[env->n_prefixes - 1].name = serd_node_copy(name);
      env->prefixes[env->n_prefixes - 1].uri  = serd_node_copy(uri);
    }
  }
}

SerdStatus
serd_env_set_prefix(SerdEnv* const        env,
                    const SerdNode* const name,
                    const SerdNode* const uri)
{
  assert(env);
  assert(name);
  assert(uri);

  if (!name || serd_node_type(uri) != SERD_URI) {
    return SERD_BAD_ARG;
  }

  if (serd_uri_string_has_scheme(serd_node_string(uri))) {
    // Set prefix to absolute URI
    serd_env_add(env, name, uri);
    return SERD_SUCCESS;
  }

  if (!env->base_uri_node) {
    return SERD_BAD_ARG; // Unresolvable relative URI
  }

  // Resolve relative URI and create a new node and URI for it
  const ZixStringView uri_str = serd_node_string_view(uri);
  SerdNode* const     abs_uri = serd_new_resolved_uri(uri_str, env->base_uri);

  // Set prefix to resolved (absolute) URI
  serd_env_add(env, name, abs_uri);

  serd_node_free(abs_uri);
  return SERD_SUCCESS;
}

SerdStatus
serd_env_set_prefix_from_strings(SerdEnv* const    env,
                                 const char* const name,
                                 const char* const uri)
{
  assert(env);
  assert(name);
  assert(uri);

  SerdNode* name_node = serd_new_string(SERD_LITERAL, name);
  SerdNode* uri_node  = serd_new_string(SERD_URI, uri);

  const SerdStatus st = serd_env_set_prefix(env, name_node, uri_node);

  serd_node_free(name_node);
  serd_node_free(uri_node);
  return st;
}

bool
serd_env_qualify(const SerdEnv* const   env,
                 const SerdNode* const  uri,
                 const SerdNode** const prefix,
                 ZixStringView* const   suffix)
{
  assert(uri);
  assert(prefix);
  assert(suffix);

  if (!env) {
    return false;
  }

  const size_t uri_len = serd_node_length(uri);

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_uri     = env->prefixes[i].uri;
    const size_t          prefix_uri_len = serd_node_length(prefix_uri);
    if (uri_len >= prefix_uri_len) {
      const char* const prefix_str = serd_node_string(prefix_uri);
      const char* const uri_str    = serd_node_string(uri);
      if (!strncmp(uri_str, prefix_str, prefix_uri_len)) {
        *prefix        = env->prefixes[i].name;
        suffix->data   = uri_str + prefix_uri_len;
        suffix->length = uri_len - prefix_uri_len;
        return true;
      }
    }
  }

  return false;
}

SerdStatus
serd_env_expand(const SerdEnv* const  env,
                const SerdNode* const curie,
                ZixStringView* const  uri_prefix,
                ZixStringView* const  uri_suffix)
{
  assert(uri_prefix);
  assert(uri_suffix);

  if (!env || !curie) {
    return SERD_BAD_CURIE;
  }

  const char* const str       = serd_node_string(curie);
  const size_t      curie_len = serd_node_length(curie);
  const char* const colon     = (const char*)memchr(str, ':', curie_len + 1);
  if (serd_node_type(curie) != SERD_CURIE || !colon) {
    return SERD_BAD_ARG;
  }

  const size_t            name_len = (size_t)(colon - str);
  const SerdPrefix* const prefix   = serd_env_find(env, str, name_len);
  if (prefix) {
    uri_prefix->data   = serd_node_string(prefix->uri);
    uri_prefix->length = prefix->uri ? serd_node_length(prefix->uri) : 0;
    uri_suffix->data   = colon + 1;
    uri_suffix->length = curie_len - name_len - 1;
    return SERD_SUCCESS;
  }

  return SERD_BAD_CURIE;
}

SerdNode*
serd_env_expand_node(const SerdEnv* const env, const SerdNode* const node)
{
  assert(node);

  if (!env) {
    return NULL;
  }

  if (serd_node_type(node) == SERD_URI) {
    return serd_new_resolved_uri(serd_node_string_view(node), env->base_uri);
  }

  if (serd_node_type(node) == SERD_CURIE) {
    ZixStringView prefix;
    ZixStringView suffix;
    if (serd_env_expand(env, node, &prefix, &suffix)) {
      return NULL;
    }

    return serd_new_expanded_uri(prefix, suffix);
  }

  return NULL;
}

void
serd_env_foreach(const SerdEnv* const env,
                 const SerdPrefixFunc func,
                 void* const          handle)
{
  assert(env);
  assert(func);

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    func(handle, env->prefixes[i].name, env->prefixes[i].uri);
  }
}
