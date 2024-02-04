// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sink.h"

#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/uri.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/filesystem.h"
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
  SerdSink      sink;
  SerdPrefix*   prefixes;
  size_t        n_prefixes;
  SerdNode*     base_uri_node;
  SerdURIView   base_uri;
};

static SerdStatus
serd_env_on_event(void* const handle, const SerdEvent* const event)
{
  SerdEnv* const env = (SerdEnv*)handle;
  SerdStatus     st  = SERD_SUCCESS;

  if (event->type == SERD_BASE) {
    st = serd_env_set_base_uri(env, serd_node_string_view(event->base.uri));
  } else if (event->type == SERD_PREFIX) {
    st = serd_env_set_prefix(env,
                             serd_node_string_view(event->prefix.name),
                             serd_node_string_view(event->prefix.uri));
  }

  return st;
}

SerdEnv*
serd_env_new(ZixAllocator* const allocator, const ZixStringView base_uri)
{
  SerdEnv* env = (SerdEnv*)zix_calloc(allocator, 1, sizeof(struct SerdEnvImpl));

  if (env) {
    env->allocator = allocator;

    env->sink.allocator   = allocator;
    env->sink.handle      = env;
    env->sink.free_handle = NULL;
    env->sink.on_event    = serd_env_on_event;

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

  if (!copy) {
    return NULL;
  }

  copy->allocator = allocator;
  if (!(copy->prefixes = (SerdPrefix*)zix_calloc(
          allocator, env->n_prefixes, sizeof(SerdPrefix))) ||
      serd_env_set_base_uri(copy, serd_env_base_uri_string(env))) {
    serd_env_free(copy);
    return NULL;
  }

  copy->n_prefixes = env->n_prefixes;
  for (size_t i = 0; i < copy->n_prefixes; ++i) {
    if (!(copy->prefixes[i].name =
            serd_node_copy(allocator, env->prefixes[i].name)) ||
        !(copy->prefixes[i].uri =
            serd_node_copy(allocator, env->prefixes[i].uri))) {
      serd_env_free(copy);
      return NULL;
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

const SerdSink*
serd_env_sink(SerdEnv* const env)
{
  return &env->sink;
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

ZixStringView
serd_env_base_uri_string(const SerdEnv* const env)
{
  return (env && env->base_uri_node) ? serd_node_string_view(env->base_uri_node)
                                     : zix_empty_string();
}

SerdURIView
serd_env_base_uri_view(const SerdEnv* const env)
{
  return env ? env->base_uri : SERD_URI_NULL;
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

  if (!new_base_uri.scheme.length) {
    return SERD_BAD_ARG;
  }

  // Replace the current base URI
  SerdNode* const new_base =
    serd_node_new(env->allocator, serd_a_parsed_uri(new_base_uri));
  if (!new_base) {
    return SERD_BAD_ALLOC;
  }

  env->base_uri_node = new_base;
  env->base_uri      = serd_node_uri_view(env->base_uri_node);
  serd_node_free(env->allocator, old_base_uri);
  return SERD_SUCCESS;
}

SerdStatus
serd_env_set_base_path(SerdEnv* const env, const ZixStringView path)
{
  assert(env);

  if (!path.length) {
    return serd_env_set_base_uri(env, zix_empty_string());
  }

  char* const real_path = zix_canonical_path(env->allocator, path.data);
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

    base_node =
      serd_node_new(env->allocator,
                    serd_a_file_uri(zix_string(base_path), zix_empty_string()));

    zix_free(env->allocator, base_path);
  } else {
    base_node =
      serd_node_new(env->allocator,
                    serd_a_file_uri(zix_string(real_path), zix_empty_string()));
  }

  serd_env_set_base_uri(env, serd_node_string_view(base_node));
  serd_node_free(env->allocator, base_node);
  zix_free(env->allocator, real_path);

  return SERD_SUCCESS;
}

ZixStringView
serd_env_get_prefix(const SerdEnv* const env, const ZixStringView name)
{
  if (!env) {
    return zix_empty_string();
  }

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_name = env->prefixes[i].name;
    if (serd_node_length(prefix_name) == name.length) {
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
    if (serd_node_length(prefix_name) == name_len) {
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
      prefix->uri = serd_node_new(env->allocator, serd_a_uri(uri));
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

    SerdNode* const name_node =
      serd_node_new(env->allocator, serd_a_string_view(name));
    SerdNode* const uri_node = serd_node_new(env->allocator, serd_a_uri(uri));
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
    return SERD_BAD_ARG; // Unresolvable relative URI
  }

  // Resolve potentially relative URI reference to an absolute URI
  const SerdURIView uri_view     = serd_parse_uri(uri.data);
  const SerdURIView abs_uri_view = serd_resolve_uri(uri_view, env->base_uri);
  assert(abs_uri_view.scheme.length);

  // Create a new node for the absolute URI
  SerdNode* const abs_uri =
    serd_node_new(env->allocator, serd_a_parsed_uri(abs_uri_view));
  if (!abs_uri) {
    return SERD_BAD_ALLOC;
  }

  assert(serd_uri_string_has_scheme(serd_node_string(abs_uri)));

  // Set prefix to resolved (absolute) URI
  const SerdStatus st = serd_env_add(env, name, serd_node_string_view(abs_uri));
  serd_node_free(env->allocator, abs_uri);
  return st;
}

SerdStatus
serd_env_qualify(const SerdEnv* const env,
                 const ZixStringView  uri,
                 ZixStringView* const prefix,
                 ZixStringView* const suffix)
{
  assert(prefix);
  assert(suffix);

  if (!env) {
    return SERD_FAILURE;
  }

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdNode* const prefix_uri     = env->prefixes[i].uri;
    const size_t          prefix_uri_len = serd_node_length(prefix_uri);
    if (uri.length >= prefix_uri_len) {
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
serd_env_expand(const SerdEnv* const env,
                const ZixStringView  curie,
                ZixStringView* const uri_prefix,
                ZixStringView* const uri_suffix)
{
  assert(uri_prefix);
  assert(uri_suffix);

  if (!env) {
    return SERD_FAILURE;
  }

  const char* const str = curie.data;
  const char* const colon =
    str ? (const char*)memchr(str, ':', curie.length + 1U) : NULL;

  const size_t            name_len = (size_t)(colon - str);
  const SerdPrefix* const prefix   = serd_env_find(env, str, name_len);
  if (!prefix) {
    return SERD_BAD_CURIE;
  }

  uri_prefix->data   = prefix->uri ? serd_node_string(prefix->uri) : "";
  uri_prefix->length = prefix->uri ? serd_node_length(prefix->uri) : 0;
  uri_suffix->data   = colon + 1;
  uri_suffix->length = curie.length - name_len - 1;
  return SERD_SUCCESS;
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
