// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "env.h"
#include "node.h"

#include "serd/env.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
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
serd_env_new(const ZixStringView base_uri)
{
  SerdEnv* env = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));
  if (env && base_uri.length) {
    serd_env_set_base_uri(env, base_uri);
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

SerdURIView
serd_env_base_uri_view(const SerdEnv* const env)
{
  assert(env);
  return env->base_uri;
}

const SerdNode*
serd_env_base_uri(const SerdEnv* const env)
{
  return env ? env->base_uri_node : NULL;
}

SerdStatus
serd_env_set_base_uri(SerdEnv* const env, const ZixStringView uri)
{
  assert(env);

  if (!uri.length) {
    serd_node_free(env->base_uri_node);
    env->base_uri_node = NULL;
    env->base_uri      = SERD_URI_NULL;
    return SERD_SUCCESS;
  }

  SerdNode* const old_base_uri = env->base_uri_node;

  // Resolve the new base against the current base in case it is relative
  const SerdURIView new_base_uri =
    serd_resolve_uri(serd_parse_uri(uri.data), env->base_uri);

  // Replace the current base URI
  env->base_uri_node = serd_new_parsed_uri(new_base_uri);
  env->base_uri      = serd_node_uri_view(env->base_uri_node);

  serd_node_free(old_base_uri);
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
serd_env_add(SerdEnv* const      env,
             const ZixStringView name,
             const ZixStringView uri)
{
  SerdPrefix* const prefix = serd_env_find(env, name.data, name.length);
  if (prefix) {
    if (!!strcmp(serd_node_string(prefix->uri), uri.data)) {
      serd_node_free(prefix->uri);
      prefix->uri = serd_new_uri(uri);
    }
  } else {
    SerdPrefix* const new_prefixes = (SerdPrefix*)realloc(
      env->prefixes, (++env->n_prefixes) * sizeof(SerdPrefix));
    if (new_prefixes) {
      env->prefixes                           = new_prefixes;
      env->prefixes[env->n_prefixes - 1].name = serd_new_string(name);
      env->prefixes[env->n_prefixes - 1].uri  = serd_new_uri(uri);
    }
  }
}

SerdStatus
serd_env_set_prefix(SerdEnv* const      env,
                    const ZixStringView name,
                    const ZixStringView uri)
{
  assert(env);

  if (serd_uri_string_has_scheme(uri.data)) {
    // Set prefix to absolute URI
    serd_env_add(env, name, uri);
    return SERD_SUCCESS;
  }

  if (!env->base_uri_node) {
    return SERD_BAD_ARG; // Unresolvable relative URI
  }

  // Resolve relative URI and create a new node and URI for it
  SerdNode* const abs_uri = serd_new_resolved_uri(uri, env->base_uri);
  assert(abs_uri);

  // Set prefix to resolved (absolute) URI
  serd_env_add(env, name, serd_node_string_view(abs_uri));
  serd_node_free(abs_uri);
  return SERD_SUCCESS;
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

SerdStatus
serd_env_describe(const SerdEnv* const env, const SerdSink* const sink)
{
  assert(env);
  assert(sink);

  SerdStatus st = SERD_SUCCESS;

  for (size_t i = 0; !st && i < env->n_prefixes; ++i) {
    const SerdPrefix* const prefix = &env->prefixes[i];

    st = serd_sink_write_prefix(sink, prefix->name, prefix->uri);
  }

  return st;
}
