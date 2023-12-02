// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/env.h"

#include "env.h"
#include "node.h"

#include "serd/node.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
  SerdNode* name;
  SerdNode* uri;
} SerdPrefix;

struct SerdEnvImpl {
  ZixAllocator* allocator;
  SerdPrefix*   prefixes;
  size_t        n_prefixes;
  SerdNode*     base_uri_node;
  SerdURIView   base_uri;
};

SerdEnv*
serd_env_new(ZixAllocator* const allocator, const ZixStringView base_uri)
{
  SerdEnv* env = (SerdEnv*)zix_calloc(allocator, 1, sizeof(struct SerdEnvImpl));

  if (env) {
    env->allocator = allocator;

    if (base_uri.length) {
      if (serd_env_set_base_uri(env, base_uri)) {
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

  SerdEnv* const copy =
    (SerdEnv*)zix_calloc(allocator, 1, sizeof(struct SerdEnvImpl));

  if (copy) {
    copy->allocator  = allocator;
    copy->n_prefixes = env->n_prefixes;

    if (!(copy->prefixes = (SerdPrefix*)zix_calloc(
            allocator, copy->n_prefixes, sizeof(SerdPrefix)))) {
      zix_free(allocator, copy);
      return NULL;
    }

    for (size_t i = 0; i < copy->n_prefixes; ++i) {
      if (!(copy->prefixes[i].name =
              serd_node_copy(allocator, env->prefixes[i].name)) ||
          !(copy->prefixes[i].uri =
              serd_node_copy(allocator, env->prefixes[i].uri))) {
        serd_env_free(copy);
        return NULL;
      }
    }

    if (env->base_uri_node) {
      SerdNode* const base_node = serd_node_copy(allocator, env->base_uri_node);
      if (!base_node) {
        serd_env_free(copy);
        return NULL;
      }

      copy->base_uri_node = base_node;
      copy->base_uri = serd_parse_uri(serd_node_string(copy->base_uri_node));
    }
  }

  return copy;
}

void
serd_env_free(SerdEnv* const env)
{
  if (!env) {
    return;
  }

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    serd_node_free(env->allocator, env->prefixes[i].name);
    serd_node_free(env->allocator, env->prefixes[i].uri);
  }
  zix_free(env->allocator, env->prefixes);
  serd_node_free(env->allocator, env->base_uri_node);
  zix_free(env->allocator, env);
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
    serd_node_free(env->allocator, env->base_uri_node);
    env->base_uri_node = NULL;
    env->base_uri      = SERD_URI_NULL;
    return SERD_SUCCESS;
  }

  SerdNode* const old_base_uri = env->base_uri_node;

  // Resolve the new base against the current base in case it is relative
  const SerdURIView new_base_uri =
    serd_resolve_uri(serd_parse_uri(uri.data), env->base_uri);

  // Replace the current base URI
  if ((env->base_uri_node =
         serd_new_parsed_uri(env->allocator, new_base_uri))) {
    env->base_uri = serd_node_uri_view(env->base_uri_node);
  } else {
    return SERD_BAD_ALLOC;
  }

  serd_node_free(env->allocator, old_base_uri);
  return SERD_SUCCESS;
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
serd_env_add(SerdEnv* const      env,
             const ZixStringView name,
             const ZixStringView uri)
{
  SerdPrefix* const prefix = serd_env_find(env, name.data, name.length);
  if (prefix) {
    if (!!strcmp(serd_node_string(prefix->uri), uri.data)) {
      serd_node_free(env->allocator, prefix->uri);
      prefix->uri = serd_new_uri(env->allocator, uri);
    }
  } else {
    SerdPrefix* const new_prefixes =
      (SerdPrefix*)zix_realloc(env->allocator,
                               env->prefixes,
                               (env->n_prefixes + 1) * sizeof(SerdPrefix));
    if (!new_prefixes) {
      return SERD_BAD_ALLOC;
    }

    env->prefixes = new_prefixes;

    SerdNode* const name_node = serd_new_string(env->allocator, name);
    SerdNode* const uri_node  = serd_new_uri(env->allocator, uri);
    if (!name_node || !uri_node) {
      serd_node_free(env->allocator, uri_node);
      serd_node_free(env->allocator, name_node);
      return SERD_BAD_ALLOC;
    }

    new_prefixes[env->n_prefixes].name = name_node;
    new_prefixes[env->n_prefixes].uri  = uri_node;
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
    return serd_env_add(env, name, uri);
  }

  if (!env->base_uri_node) {
    return SERD_BAD_ARG;
  }

  // Resolve relative URI and create a new node and URI for it
  SerdNode* const abs_uri =
    serd_new_resolved_uri(env->allocator, uri, env->base_uri);
  if (!abs_uri) {
    return SERD_BAD_ALLOC;
  }

  // Set prefix to resolved (absolute) URI
  const SerdStatus st = serd_env_add(env, name, serd_node_string_view(abs_uri));
  serd_node_free(env->allocator, abs_uri);
  return st;
}

bool
serd_env_qualify_in_place(const SerdEnv* const   env,
                          const SerdNode* const  uri,
                          const SerdNode** const prefix,
                          ZixStringView* const   suffix)
{
  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_uri = env->prefixes[i].uri;
    if (uri->length >= prefix_uri->length) {
      const char* prefix_str = serd_node_string(prefix_uri);
      const char* uri_str    = serd_node_string(uri);

      if (!strncmp(uri_str, prefix_str, prefix_uri->length)) {
        *prefix        = env->prefixes[i].name;
        suffix->data   = uri_str + prefix_uri->length;
        suffix->length = uri->length - prefix_uri->length;
        return true;
      }
    }
  }
  return false;
}

SerdNode*
serd_env_qualify(const SerdEnv* const env, const SerdNode* const uri)
{
  if (!env || !uri) {
    return NULL;
  }

  const SerdNode* prefix = NULL;
  ZixStringView   suffix = {NULL, 0};
  if (serd_env_qualify_in_place(env, uri, &prefix, &suffix)) {
    const size_t prefix_len = serd_node_length(prefix);
    const size_t length     = prefix_len + 1 + suffix.length;
    SerdNode*    node = serd_node_malloc(env->allocator, length, 0, SERD_CURIE);

    memcpy(serd_node_buffer(node), serd_node_string(prefix), prefix_len);
    serd_node_buffer(node)[prefix_len] = ':';
    memcpy(serd_node_buffer(node) + 1 + prefix_len, suffix.data, suffix.length);
    node->length = length;
    return node;
  }

  return NULL;
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
  if (prefix) {
    uri_prefix->data   = serd_node_string(prefix->uri);
    uri_prefix->length = prefix->uri ? prefix->uri->length : 0;
    uri_suffix->data   = colon + 1;
    uri_suffix->length = curie.length - name_len - 1;
    return SERD_SUCCESS;
  }
  return SERD_BAD_CURIE;
}

static SerdNode*
expand_curie(const SerdEnv* const env, const SerdNode* const node)
{
  assert(serd_node_type(node) == SERD_CURIE);

  ZixStringView prefix;
  ZixStringView suffix;
  if (serd_env_expand_in_place(
        env, serd_node_string_view(node), &prefix, &suffix)) {
    return NULL;
  }

  const size_t    len = prefix.length + suffix.length;
  SerdNode* const ret = serd_node_malloc(env->allocator, len, 0U, SERD_URI);
  if (ret) {
    char* const string = serd_node_buffer(ret);
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
    break;
  case SERD_URI:
    return serd_new_resolved_uri(
      env->allocator, serd_node_string_view(node), env->base_uri);
  case SERD_CURIE:
    return expand_curie(env, node);
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
