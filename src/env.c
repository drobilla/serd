// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/env.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  SerdNode name;
  SerdNode uri;
} SerdPrefix;

struct SerdEnvImpl {
  SerdPrefix* prefixes;
  size_t      n_prefixes;
  SerdNode    base_uri_node;
  SerdURIView base_uri;
};

SerdEnv*
serd_env_new(const SerdNode* const base_uri)
{
  SerdEnv* env = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));
  if (env && base_uri && base_uri->type != SERD_NOTHING) {
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
    serd_node_free(&env->prefixes[i].name);
    serd_node_free(&env->prefixes[i].uri);
  }

  free(env->prefixes);
  serd_node_free(&env->base_uri_node);
  free(env);
}

const SerdNode*
serd_env_base_uri(const SerdEnv* const env, SerdURIView* const out)
{
  assert(env);

  if (out) {
    *out = env->base_uri;
  }

  return &env->base_uri_node;
}

SerdStatus
serd_env_set_base_uri(SerdEnv* const env, const SerdNode* const uri)
{
  assert(env);

  if (uri && uri->type != SERD_URI) {
    return SERD_BAD_ARG;
  }

  if (!uri || !uri->buf) {
    serd_node_free(&env->base_uri_node);
    env->base_uri_node = SERD_NODE_NULL;
    env->base_uri      = SERD_URI_NULL;
    return SERD_SUCCESS;
  }

  // Resolve base URI and create a new node and URI for it
  SerdURIView base_uri;
  SerdNode    base_uri_node =
    serd_node_new_uri_from_node(uri, &env->base_uri, &base_uri);

  // Replace the current base URI
  serd_node_free(&env->base_uri_node);
  env->base_uri_node = base_uri_node;
  env->base_uri      = base_uri;

  return SERD_SUCCESS;
}

SERD_PURE_FUNC static SerdPrefix*
serd_env_find(const SerdEnv* const env,
              const char* const    name,
              const size_t         name_len)
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
serd_env_add(SerdEnv* const        env,
             const SerdNode* const name,
             const SerdNode* const uri)
{
  SerdPrefix* const prefix = serd_env_find(env, name->buf, name->n_bytes);
  if (prefix) {
    if (!serd_node_equals(&prefix->uri, uri)) {
      SerdNode old_prefix_uri = prefix->uri;
      prefix->uri             = serd_node_copy(uri);
      serd_node_free(&old_prefix_uri);
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

  if (!name->buf || uri->type != SERD_URI) {
    return SERD_BAD_ARG;
  }

  if (serd_uri_string_has_scheme(uri->buf)) {
    // Set prefix to absolute URI
    serd_env_add(env, name, uri);
  } else {
    // Resolve relative URI and create a new node and URI for it
    SerdURIView abs_uri;
    SerdNode    abs_uri_node =
      serd_node_new_uri_from_node(uri, &env->base_uri, &abs_uri);

    // Set prefix to resolved (absolute) URI
    serd_env_add(env, name, &abs_uri_node);
    serd_node_free(&abs_uri_node);
  }

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

  const SerdNode name_node = serd_node_from_string(SERD_LITERAL, name);
  const SerdNode uri_node  = serd_node_from_string(SERD_URI, uri);

  return serd_env_set_prefix(env, &name_node, &uri_node);
}

bool
serd_env_qualify(const SerdEnv* const  env,
                 const SerdNode* const uri,
                 SerdNode* const       prefix,
                 SerdStringView* const suffix)
{
  assert(uri);
  assert(prefix);
  assert(suffix);

  if (!env) {
    return false;
  }

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_uri = &env->prefixes[i].uri;
    if (uri->n_bytes >= prefix_uri->n_bytes) {
      if (!strncmp(uri->buf, prefix_uri->buf, prefix_uri->n_bytes)) {
        *prefix        = env->prefixes[i].name;
        suffix->data   = (const char*)uri->buf + prefix_uri->n_bytes;
        suffix->length = uri->n_bytes - prefix_uri->n_bytes;
        return true;
      }
    }
  }
  return false;
}

SerdStatus
serd_env_expand(const SerdEnv* const  env,
                const SerdNode* const curie,
                SerdStringView* const uri_prefix,
                SerdStringView* const uri_suffix)
{
  assert(curie);
  assert(uri_prefix);
  assert(uri_suffix);

  if (!env) {
    return SERD_BAD_CURIE;
  }

  const char* const colon =
    (const char*)memchr(curie->buf, ':', curie->n_bytes + 1);
  if (curie->type != SERD_CURIE || !colon) {
    return SERD_BAD_ARG;
  }

  const size_t            name_len = (size_t)(colon - curie->buf);
  const SerdPrefix* const prefix   = serd_env_find(env, curie->buf, name_len);
  if (prefix) {
    uri_prefix->data   = (const char*)prefix->uri.buf;
    uri_prefix->length = prefix->uri.n_bytes;
    uri_suffix->data   = colon + 1;
    uri_suffix->length = curie->n_bytes - name_len - 1;
    return SERD_SUCCESS;
  }
  return SERD_BAD_CURIE;
}

SerdNode
serd_env_expand_node(const SerdEnv* const env, const SerdNode* const node)
{
  assert(node);

  if (!env) {
    return SERD_NODE_NULL;
  }

  if (node->type == SERD_URI) {
    SerdURIView ignored;
    return serd_node_new_uri_from_node(node, &env->base_uri, &ignored);
  }

  if (node->type == SERD_CURIE) {
    SerdStringView prefix;
    SerdStringView suffix;
    if (serd_env_expand(env, node, &prefix, &suffix)) {
      return SERD_NODE_NULL;
    }

    const size_t len = prefix.length + suffix.length;
    char*        buf = (char*)malloc(len + 1);
    SerdNode     ret = {buf, len, 0, SERD_URI};
    snprintf(buf, ret.n_bytes + 1, "%s%s", prefix.data, suffix.data);
    return ret;
  }

  return SERD_NODE_NULL;
}

void
serd_env_foreach(const SerdEnv* const env,
                 const SerdPrefixFunc func,
                 void* const          handle)
{
  assert(env);
  assert(func);

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    func(handle, &env->prefixes[i].name, &env->prefixes[i].uri);
  }
}
