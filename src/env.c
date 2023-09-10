// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/env.h"

#include "env.h"
#include "node.h"

#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/write_result.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/filesystem.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
  const SerdNode* name;
  const SerdNode* uri;
} SerdPrefix;

struct SerdEnvImpl {
  ZixAllocator*   allocator;
  SerdNodes*      nodes;
  SerdPrefix*     prefixes;
  size_t          n_prefixes;
  const SerdNode* base_uri_node;
  SerdURIView     base_uri;
};

SerdEnv*
serd_env_new(ZixAllocator* const allocator, const ZixStringView base_uri)
{
  SerdEnv* env = (SerdEnv*)zix_calloc(allocator, 1, sizeof(struct SerdEnvImpl));

  if (env) {
    env->allocator = allocator;
    if (!(env->nodes = serd_nodes_new(allocator))) {
      zix_free(allocator, env);
      return NULL;
    }

    if (base_uri.length) {
      if (serd_env_set_base_uri(env, base_uri)) {
        serd_nodes_free(env->nodes);
        zix_free(allocator, env);
        return NULL;
      }
    }
  }

  return env;
}

SerdEnv*
serd_env_copy(ZixAllocator* const allocator, const SerdEnv* const env)
{
  if (!env) {
    return NULL;
  }

  SerdEnv* const copy = (SerdEnv*)zix_calloc(allocator, 1, sizeof(SerdEnv));
  if (copy) {
    copy->allocator  = allocator;
    copy->n_prefixes = env->n_prefixes;

    // Allocate structure and set base URI
    const SerdNode* const base = env->base_uri_node;
    if (!(copy->nodes = serd_nodes_new(allocator)) ||
        !(copy->prefixes = (SerdPrefix*)zix_calloc(
            allocator, copy->n_prefixes, sizeof(SerdPrefix))) ||
        (base && serd_env_set_base_uri(copy, serd_node_string_view(base)))) {
      serd_env_free(copy);
      return NULL;
    }

    // Copy each namespace prefix
    for (size_t i = 0; i < copy->n_prefixes; ++i) {
      if (!(copy->prefixes[i].name =
              serd_nodes_intern(copy->nodes, env->prefixes[i].name)) ||
          !(copy->prefixes[i].uri =
              serd_nodes_intern(copy->nodes, env->prefixes[i].uri))) {
        serd_env_free(copy);
        return NULL;
      }
    }
  }
  return copy;
}

void
serd_env_free(SerdEnv* const env)
{
  if (env) {
    zix_free(env->allocator, env->prefixes);
    serd_nodes_free(env->nodes);
    zix_free(env->allocator, env);
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
serd_env_set_base_uri(SerdEnv* const env, const ZixStringView uri)
{
  assert(env);

  if (!uri.length) {
    serd_nodes_deref(env->nodes, env->base_uri_node);
    env->base_uri_node = NULL;
    env->base_uri      = SERD_URI_NULL;
    return SERD_SUCCESS;
  }

  const SerdNode* const old_base_uri = env->base_uri_node;

  // Resolve the new base against the current base in case it is relative
  const SerdURIView new_base_uri =
    serd_resolve_uri(serd_parse_uri(uri.data), env->base_uri);

  // Replace the current base URI
  if ((env->base_uri_node =
         serd_nodes_get(env->nodes, serd_a_parsed_uri(new_base_uri)))) {
    env->base_uri = serd_node_uri_view(env->base_uri_node);
  } else {
    return SERD_BAD_ALLOC;
  }

  serd_nodes_deref(env->nodes, old_base_uri);
  return SERD_SUCCESS;
}

SerdStatus
serd_env_set_base_path(SerdEnv* const env, const ZixStringView path)
{
  assert(env);

  if (!path.data || !path.length) {
    return serd_env_set_base_uri(env, zix_empty_string());
  }

  char* const real_path = zix_canonical_path(NULL, path.data);
  if (!real_path) {
    return SERD_BAD_ARG;
  }

  const size_t real_path_len = strlen(real_path);
  SerdNode*    base_node     = NULL;
  const char   path_last     = path.data[path.length - 1];
  if (path_last == '/' || path_last == '\\') {
    char* const base_path =
      (char*)zix_calloc(env->allocator, real_path_len + 2, 1);

    memcpy(base_path, real_path, real_path_len + 1);
    base_path[real_path_len] = path_last;

    base_node = serd_node_new(
      NULL, serd_a_file_uri(zix_string(base_path), zix_empty_string()));

    zix_free(env->allocator, base_path);
  } else {
    base_node = serd_node_new(
      NULL, serd_a_file_uri(zix_string(real_path), zix_empty_string()));
  }

  serd_env_set_base_uri(env, serd_node_string_view(base_node));
  serd_node_free(NULL, base_node);
  zix_free(NULL, real_path);

  return SERD_SUCCESS;
}

ZixStringView
serd_env_find_prefix(const SerdEnv* const env, const ZixStringView name)
{
  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_name = env->prefixes[i].name;
    if (prefix_name->length == name.length) {
      if (!memcmp(serd_node_string(prefix_name), name.data, name.length)) {
        return serd_node_string_view(env->prefixes[i].uri);
      }
    }
  }

  return zix_empty_string();
}

ZIX_PURE_FUNC static SerdPrefix*
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

static SerdStatus
serd_env_add(SerdEnv* const        env,
             const ZixStringView   name,
             const SerdNode* const uri)
{
  SerdPrefix* const prefix = serd_env_find(env, name.data, name.length);
  if (prefix) {
    if (!!strcmp(serd_node_string(prefix->uri), serd_node_string(uri))) {
      serd_nodes_deref(env->nodes, prefix->uri);
      prefix->uri = uri;
    }
  } else {
    const SerdNode* const name_node =
      serd_nodes_get(env->nodes, serd_a_string_view(name));

    if (!name_node) {
      return SERD_BAD_ALLOC;
    }

    SerdPrefix* const new_prefixes =
      (SerdPrefix*)zix_realloc(env->allocator,
                               env->prefixes,
                               (env->n_prefixes + 1) * sizeof(SerdPrefix));

    if (!new_prefixes) {
      return SERD_BAD_ALLOC;
    }

    new_prefixes[env->n_prefixes].name = name_node;
    new_prefixes[env->n_prefixes].uri  = uri;
    env->prefixes                      = new_prefixes;
    ++env->n_prefixes;
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_env_set_prefix(SerdEnv* const      env,
                    const ZixStringView name,
                    const ZixStringView uri)
{
  assert(env);

  if (serd_uri_string_has_scheme(uri.data)) {
    // Set prefix to absolute URI
    const SerdNode* const abs_uri = serd_nodes_get(env->nodes, serd_a_uri(uri));
    if (!abs_uri) {
      return SERD_BAD_ALLOC;
    }

    return serd_env_add(env, name, abs_uri);
  }

  if (!env->base_uri_node) {
    return SERD_BAD_ARG;
  }

  // Resolve potentially relative URI reference to an absolute URI
  const SerdURIView uri_view     = serd_parse_uri(uri.data);
  const SerdURIView abs_uri_view = serd_resolve_uri(uri_view, env->base_uri);
  assert(abs_uri_view.scheme.length);

  // Create a new node for the absolute URI
  const SerdNode* const abs_uri =
    serd_nodes_get(env->nodes, serd_a_parsed_uri(abs_uri_view));

  if (!abs_uri) {
    return SERD_BAD_ALLOC;
  }

  assert(serd_uri_string_has_scheme(serd_node_string(abs_uri)));

  // Set prefix to resolved (absolute) URI
  return serd_env_add(env, name, abs_uri);
}

SerdStatus
serd_env_qualify(const SerdEnv* const env,
                 const ZixStringView  uri,
                 ZixStringView* const prefix,
                 ZixStringView* const suffix)
{
  if (!env) {
    return SERD_FAILURE;
  }

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_uri     = env->prefixes[i].uri;
    const size_t          prefix_uri_len = serd_node_length(prefix_uri);
    if (uri.data && uri.length >= prefix_uri_len) {
      const char* prefix_str = serd_node_string(prefix_uri);
      const char* uri_str    = uri.data;

      if (!strncmp(uri_str, prefix_str, prefix_uri_len)) {
        *prefix        = serd_node_string_view(env->prefixes[i].name);
        suffix->data   = uri_str + prefix_uri_len;
        suffix->length = uri.length - prefix_uri_len;
        return SERD_SUCCESS;
      }
    }
  }

  return SERD_FAILURE;
}

SerdStatus
serd_env_expand_in_place(const SerdEnv* const env,
                         const ZixStringView  curie,
                         ZixStringView* const uri_prefix,
                         ZixStringView* const uri_suffix)
{
  const char* const str = curie.data;
  const char* const colon =
    str ? (const char*)memchr(str, ':', curie.length + 1) : NULL;

  const size_t            name_len = (size_t)(colon - str);
  const SerdPrefix* const prefix   = serd_env_find(env, str, name_len);
  if (!prefix) {
    return SERD_BAD_CURIE;
  }

  uri_prefix->data   = prefix->uri ? serd_node_string(prefix->uri) : "";
  uri_prefix->length = prefix->uri ? prefix->uri->length : 0;
  uri_suffix->data   = colon + 1;
  uri_suffix->length = curie.length - name_len - 1;
  return SERD_SUCCESS;
}

static SerdNode*
serd_env_expand_uri(const SerdEnv* const env, const ZixStringView uri)
{
  const SerdURIView rel_uri = serd_parse_uri(uri.data);
  const SerdURIView abs_uri = serd_resolve_uri(rel_uri, env->base_uri);
  if (!abs_uri.scheme.length) {
    return NULL;
  }

  const SerdWriteResult r =
    serd_node_construct(0U, NULL, serd_a_parsed_uri(abs_uri));

  SerdNode* const expanded = serd_node_try_malloc(env->allocator, r);
  if (expanded) {
    serd_node_construct(r.count, expanded, serd_a_parsed_uri(abs_uri));
  }

  return expanded;
}

SerdNode*
serd_env_expand_curie(const SerdEnv* const env, const ZixStringView curie)
{
  ZixStringView prefix;
  ZixStringView suffix;
  if (serd_env_expand_in_place(env, curie, &prefix, &suffix)) {
    return NULL;
  }

  const size_t len = prefix.length + suffix.length;
  SerdNode* ret = serd_node_malloc(env->allocator, sizeof(SerdNode) + len + 1);

  if (ret) {
    ret->length = len;
    ret->flags  = 0U;
    ret->type   = SERD_URI;

    char* const string = serd_node_buffer(ret);
    assert(prefix.data);
    memcpy(string, prefix.data, prefix.length);
    memcpy(string + prefix.length, suffix.data, suffix.length);
    ret->length = len;
  }

  return ret;
}

SerdNode*
serd_env_expand_node(const SerdEnv* const env, const SerdNode* const node)
{
  if (!env || !node) {
    return NULL;
  }

  switch (node->type) {
  case SERD_LITERAL:
  case SERD_URI:
    return serd_env_expand_uri(env, serd_node_string_view(node));
  case SERD_CURIE:
    return serd_env_expand_curie(env, serd_node_string_view(node));
  case SERD_BLANK:
  case SERD_VARIABLE:
    break;
  }

  return NULL;
}

SerdStatus
serd_env_write_prefixes(const SerdEnv* const env, const SerdSink* const sink)
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
