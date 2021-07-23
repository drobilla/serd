// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/env.h"

#include "env.h"
#include "node.h"

#include "serd/node.h"
#include "serd/nodes.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const SerdNode* name;
  const SerdNode* uri;
} SerdPrefix;

struct SerdEnvImpl {
  SerdNodes*      nodes;
  SerdPrefix*     prefixes;
  size_t          n_prefixes;
  const SerdNode* base_uri_node;
  SerdURIView     base_uri;
};

SerdEnv*
serd_env_new(const SerdStringView base_uri)
{
  SerdEnv* env = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));

  env->nodes = serd_nodes_new();

  if (env && base_uri.len) {
    if (serd_env_set_base_uri(env, base_uri)) {
      free(env);
      return NULL;
    }
  }

  return env;
}

SerdEnv*
serd_env_copy(const SerdEnv* const env)
{
  if (!env) {
    return NULL;
  }

  SerdEnv* copy = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));
  if (copy) {
    copy->nodes      = serd_nodes_new();
    copy->n_prefixes = env->n_prefixes;

    copy->prefixes = (SerdPrefix*)malloc(copy->n_prefixes * sizeof(SerdPrefix));
    for (size_t i = 0; i < copy->n_prefixes; ++i) {
      copy->prefixes[i].name =
        serd_nodes_intern(copy->nodes, env->prefixes[i].name);

      copy->prefixes[i].uri =
        serd_nodes_intern(copy->nodes, env->prefixes[i].uri);
    }

    const SerdNode* const base = serd_env_base_uri(env);
    if (base) {
      serd_env_set_base_uri(copy, serd_node_string_view(base));
    }
  }

  return copy;
}

void
serd_env_free(SerdEnv* const env)
{
  if (env) {
    free(env->prefixes);
    serd_nodes_free(env->nodes);
    free(env);
  }
}

bool
serd_env_equals(const SerdEnv* const a, const SerdEnv* const b)
{
  if (!a || !b) {
    return !a == !b;
  }

  if (a->n_prefixes != b->n_prefixes ||
      !serd_node_equals(a->base_uri_node, b->base_uri_node)) {
    return false;
  }

  for (size_t i = 0; i < a->n_prefixes; ++i) {
    if (!serd_node_equals(a->prefixes[i].name, b->prefixes[i].name) ||
        !serd_node_equals(a->prefixes[i].uri, b->prefixes[i].uri)) {
      return false;
    }
  }

  return true;
}

SerdURIView
serd_env_base_uri_view(const SerdEnv* const env)
{
  return env->base_uri;
}

const SerdNode*
serd_env_base_uri(const SerdEnv* const env)
{
  return env ? env->base_uri_node : NULL;
}

SerdStatus
serd_env_set_base_uri(SerdEnv* const env, const SerdStringView uri)
{
  assert(env);

  if (!uri.len) {
    serd_nodes_deref(env->nodes, env->base_uri_node);
    env->base_uri_node = NULL;
    env->base_uri      = SERD_URI_NULL;
    return SERD_SUCCESS;
  }

  const SerdNode* const old_base_uri = env->base_uri_node;

  // Resolve the new base against the current base in case it is relative
  const SerdURIView new_base_uri =
    serd_resolve_uri(serd_parse_uri(uri.buf), env->base_uri);

  // Replace the current base URI
  env->base_uri_node = serd_nodes_parsed_uri(env->nodes, new_base_uri);
  env->base_uri      = serd_node_uri_view(env->base_uri_node);

  serd_nodes_deref(env->nodes, old_base_uri);
  return SERD_SUCCESS;
}

SERD_PURE_FUNC
static SerdPrefix*
serd_env_find(const SerdEnv* const env,
              const char* const    name,
              const size_t         name_len)
{
  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_name = env->prefixes[i].name;
    if (prefix_name->length == name_len) {
      if (!memcmp(serd_node_string(prefix_name), name, name_len)) {
        return &env->prefixes[i];
      }
    }
  }
  return NULL;
}

static void
serd_env_add(SerdEnv* const        env,
             const SerdStringView  name,
             const SerdNode* const uri)
{
  SerdPrefix* const prefix = serd_env_find(env, name.buf, name.len);
  if (prefix) {
    if (!!strcmp(serd_node_string(prefix->uri), serd_node_string(uri))) {
      serd_nodes_deref(env->nodes, prefix->uri);
      prefix->uri = uri;
    }
  } else {
    env->prefixes = (SerdPrefix*)realloc(
      env->prefixes, (++env->n_prefixes) * sizeof(SerdPrefix));

    env->prefixes[env->n_prefixes - 1].name =
      serd_nodes_string(env->nodes, name);

    env->prefixes[env->n_prefixes - 1].uri = uri;
  }
}

SerdStatus
serd_env_set_prefix(SerdEnv* const       env,
                    const SerdStringView name,
                    const SerdStringView uri)
{
  assert(env);

  if (serd_uri_string_has_scheme(uri.buf)) {
    // Set prefix to absolute URI
    serd_env_add(env, name, serd_nodes_uri(env->nodes, uri));
    return SERD_SUCCESS;
  }

  if (!env->base_uri_node) {
    return SERD_ERR_BAD_ARG;
  }

  // Resolve potentially relative URI reference to an absolute URI
  const SerdURIView uri_view     = serd_parse_uri(uri.buf);
  const SerdURIView abs_uri_view = serd_resolve_uri(uri_view, env->base_uri);
  assert(abs_uri_view.scheme.len);

  // Serialise absolute URI to a new node
  const SerdNode* const abs_uri =
    serd_nodes_parsed_uri(env->nodes, abs_uri_view);

  assert(serd_uri_string_has_scheme(serd_node_string(abs_uri)));

  // Set prefix to resolved absolute URI
  serd_env_add(env, name, abs_uri);
  return SERD_SUCCESS;
}

SerdStatus
serd_env_qualify(const SerdEnv* const  env,
                 const SerdStringView  uri,
                 SerdStringView* const prefix,
                 SerdStringView* const suffix)
{
  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_uri     = env->prefixes[i].uri;
    const size_t          prefix_uri_len = serd_node_length(prefix_uri);
    if (uri.buf && uri.len >= prefix_uri_len) {
      const char* prefix_str = serd_node_string(prefix_uri);
      const char* uri_str    = uri.buf;

      if (!strncmp(uri_str, prefix_str, prefix_uri_len)) {
        *prefix     = serd_node_string_view(env->prefixes[i].name);
        suffix->buf = uri_str + prefix_uri_len;
        suffix->len = uri.len - prefix_uri_len;
        return SERD_SUCCESS;
      }
    }
  }

  return SERD_FAILURE;
}

SerdStatus
serd_env_expand_in_place(const SerdEnv* const  env,
                         const SerdStringView  curie,
                         SerdStringView* const uri_prefix,
                         SerdStringView* const uri_suffix)
{
  const char* const str = curie.buf;
  const char* const colon =
    str ? (const char*)memchr(str, ':', curie.len + 1) : NULL;
  if (!colon) {
    return SERD_ERR_BAD_ARG;
  }

  const size_t            name_len = (size_t)(colon - str);
  const SerdPrefix* const prefix   = serd_env_find(env, str, name_len);
  if (!prefix || !prefix->uri) {
    return SERD_ERR_BAD_CURIE;
  }

  uri_prefix->buf = prefix->uri ? serd_node_string(prefix->uri) : "";
  uri_prefix->len = prefix->uri ? prefix->uri->length : 0;
  uri_suffix->buf = colon + 1;
  uri_suffix->len = curie.len - name_len - 1;
  return SERD_SUCCESS;
}

SerdNode*
serd_env_expand_curie(const SerdEnv* const env, const SerdStringView curie)
{
  if (!env) {
    return NULL;
  }

  SerdStringView prefix = serd_empty_string();
  SerdStringView suffix = serd_empty_string();
  SerdStatus     st = serd_env_expand_in_place(env, curie, &prefix, &suffix);
  if (st || !prefix.buf || !suffix.buf) {
    return NULL;
  }

  const size_t len = prefix.len + suffix.len;
  SerdNode*    ret = serd_node_malloc(sizeof(SerdNode) + len + 1U);
  if (ret) {
    ret->length = len;
    ret->flags  = 0U;
    ret->type   = SERD_URI;

    char* const string = serd_node_buffer(ret);
    assert(prefix.buf);
    memcpy(string, prefix.buf, prefix.len);
    memcpy(string + prefix.len, suffix.buf, suffix.len);
  }

  return ret;
}

SerdNode*
serd_env_expand_node(const SerdEnv* const env, const SerdNode* const node)
{
  if (!env || !node || node->type != SERD_URI) {
    return NULL;
  }

  const SerdURIView uri     = serd_node_uri_view(node);
  const SerdURIView abs_uri = serd_resolve_uri(uri, env->base_uri);
  if (!abs_uri.scheme.len) {
    return NULL;
  }

  const SerdWriteResult r        = serd_node_construct_uri(0U, NULL, abs_uri);
  SerdNode* const       expanded = serd_node_try_malloc(r);
  if (expanded) {
    serd_node_construct_uri(r.count, expanded, abs_uri);
  }

  return expanded;
}

void
serd_env_write_prefixes(const SerdEnv* const env, const SerdSink* const sink)
{
  assert(env);
  assert(sink);

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    serd_sink_write_prefix(sink, env->prefixes[i].name, env->prefixes[i].uri);
  }
}
