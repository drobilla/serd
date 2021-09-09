/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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

#include "env.h"

#include "node.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const SerdNode* name;
  const SerdNode* uri;
} SerdEnvEntry;

struct SerdEnvImpl {
  SerdNodes*      nodes;
  SerdEnvEntry*   entries;
  size_t          n_entries;
  const SerdNode* base_uri_node;
  SerdURIView     base_uri;
};

SerdEnv*
serd_env_new(const SerdStringView base_uri)
{
  SerdEnv* env = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));

  env->nodes = serd_nodes_new();

  if (env && base_uri.len) {
    serd_env_set_base_uri(env, base_uri);
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
    copy->nodes     = serd_nodes_new();
    copy->n_entries = env->n_entries;

    copy->entries =
      (SerdEnvEntry*)malloc(copy->n_entries * sizeof(SerdEnvEntry));
    for (size_t i = 0; i < copy->n_entries; ++i) {
      copy->entries[i].name =
        serd_nodes_intern(copy->nodes, env->entries[i].name);

      copy->entries[i].uri =
        serd_nodes_intern(copy->nodes, env->entries[i].uri);
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
    free(env->entries);
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

  if (a->n_entries != b->n_entries ||
      !serd_node_equals(a->base_uri_node, b->base_uri_node)) {
    return false;
  }

  for (size_t i = 0; i < a->n_entries; ++i) {
    if (!serd_node_equals(a->entries[i].name, b->entries[i].name) ||
        !serd_node_equals(a->entries[i].uri, b->entries[i].uri)) {
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
static SerdEnvEntry*
serd_env_find(const SerdEnv* const env,
              const char* const    name,
              const size_t         name_len)
{
  for (size_t i = 0; i < env->n_entries; ++i) {
    const SerdNode* const entry_name = env->entries[i].name;
    if (entry_name->length == name_len) {
      if (!memcmp(serd_node_string(entry_name), name, name_len)) {
        return &env->entries[i];
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
  SerdEnvEntry* const entry = serd_env_find(env, name.buf, name.len);
  if (entry) {
    if (strcmp(serd_node_string(entry->uri), serd_node_string(uri))) {
      serd_nodes_deref(env->nodes, entry->uri);
      entry->uri = uri;
    }
  } else {
    env->entries = (SerdEnvEntry*)realloc(
      env->entries, (++env->n_entries) * sizeof(SerdEnvEntry));

    env->entries[env->n_entries - 1].name = serd_nodes_string(env->nodes, name);

    env->entries[env->n_entries - 1].uri = uri;
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

bool
serd_env_qualify_in_place(const SerdEnv* const   env,
                          const SerdNode* const  uri,
                          const SerdNode** const prefix,
                          SerdStringView* const  suffix)
{
  for (size_t i = 0; i < env->n_entries; ++i) {
    const SerdNode* const prefix_uri = env->entries[i].uri;
    if (uri->length >= prefix_uri->length) {
      const char* prefix_str = serd_node_string(prefix_uri);
      const char* uri_str    = serd_node_string(uri);

      if (!strncmp(uri_str, prefix_str, prefix_uri->length)) {
        *prefix     = env->entries[i].name;
        suffix->buf = uri_str + prefix_uri->length;
        suffix->len = uri->length - prefix_uri->length;
        return true;
      }
    }
  }
  return false;
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

  const size_t              name_len = (size_t)(colon - str);
  const SerdEnvEntry* const prefix   = serd_env_find(env, str, name_len);
  if (prefix) {
    uri_prefix->buf = prefix->uri ? serd_node_string(prefix->uri) : "";
    uri_prefix->len = prefix->uri ? prefix->uri->length : 0;
    uri_suffix->buf = colon + 1;
    uri_suffix->len = curie.len - name_len - 1;
    return SERD_SUCCESS;
  }

  return SERD_ERR_BAD_CURIE;
}

SerdNode*
serd_env_expand_curie(const SerdEnv* const env, const SerdStringView curie)
{
  if (!env) {
    return NULL;
  }

  SerdStringView prefix;
  SerdStringView suffix;
  if (serd_env_expand_in_place(env, curie, &prefix, &suffix)) {
    return NULL;
  }

  const size_t len = prefix.len + suffix.len;
  SerdNode*    ret = serd_node_malloc(sizeof(SerdNode) + len + 1);
  if (ret) {
    ret->length = len;
    ret->flags  = 0u;
    ret->type   = SERD_URI;

    char* const string = serd_node_buffer(ret);
    assert(prefix.buf);
    memcpy(string, prefix.buf, prefix.len);
    memcpy(string + prefix.len, suffix.buf, suffix.len);
  }

  return ret;
}

SerdNode*
serd_env_expand(const SerdEnv* env, const SerdNode* node)
{
  if (!env || !node || node->type != SERD_URI) {
    return NULL;
  }

  const SerdURIView uri     = serd_node_uri_view(node);
  const SerdURIView abs_uri = serd_resolve_uri(uri, env->base_uri);
  if (!abs_uri.scheme.len) {
    return NULL;
  }

  const SerdWriteResult r        = serd_node_construct_uri(0u, NULL, abs_uri);
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

  for (size_t i = 0; i < env->n_entries; ++i) {
    serd_sink_write_prefix(sink, env->entries[i].name, env->entries[i].uri);
  }
}
