// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/env.h>

#include <serd/event.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <serd/string.h>
#include <serd/string_pair_view.h>
#include <serd/uri.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <string.h>

typedef struct {
  SerdString name;
  SerdString uri;
} SerdPrefix;

struct SerdEnvImpl {
  SerdSink      sink;
  ZixAllocator* allocator;
  SerdPrefix*   prefixes;
  size_t        n_prefixes;
  SerdString    base_uri_string;
  SerdURIView   base_uri;
};

static SerdStatus
serd_env_on_event(void* handle, const SerdEvent* event);

static SerdString
copy_string_view(ZixAllocator* const allocator, const ZixStringView view)
{
  char* const      data   = zix_string_view_copy(allocator, view);
  const SerdString string = {data ? view.length : 0U, data};
  return string;
}

SerdEnv*
serd_env_new(ZixAllocator* const allocator, const ZixStringView base_uri)
{
  SerdEnv* const env =
    (SerdEnv*)zix_calloc(allocator, 1, sizeof(struct SerdEnvImpl));

  if (env) {
    env->sink.handle   = env;
    env->sink.on_event = serd_env_on_event;
    env->allocator     = allocator;

    if (base_uri.length) {
      if (serd_env_set_base_uri(env, base_uri)) {
        zix_free(allocator, env);
        return NULL;
      }
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
    zix_free(env->allocator, env->prefixes[i].name.data);
    zix_free(env->allocator, env->prefixes[i].uri.data);
  }
  zix_free(env->allocator, env->prefixes);
  zix_free(env->allocator, env->base_uri_string.data);
  zix_free(env->allocator, env);
}

const SerdSink*
serd_env_sink(SerdEnv* const env)
{
  return &env->sink;
}

ZixStringView
serd_env_base_uri_string(const SerdEnv* const env)
{
  return env ? serd_string_view(env->base_uri_string) : zix_empty_string();
}

SerdURIView
serd_env_base_uri_view(const SerdEnv* const env)
{
  return env ? env->base_uri : serd_empty_uri();
}

SerdStatus
serd_env_set_base_uri(SerdEnv* const env, const ZixStringView uri)
{
  assert(env);

  if (!uri.length) {
    zix_free(env->allocator, env->base_uri_string.data);
    env->base_uri_string.length = 0U;
    env->base_uri_string.data   = NULL;
    env->base_uri               = serd_empty_uri();
    return SERD_SUCCESS;
  }

  SerdString old_base_string = env->base_uri_string;

  // Resolve the new base against the current base in case it's relative
  const SerdURIView parsed       = serd_parse_uri(uri.data);
  SerdURIView       new_base_uri = serd_resolve_uri(parsed, env->base_uri);
  if (!serd_uri_has_scheme(new_base_uri)) {
    return SERD_BAD_ARG;
  }

  // Replace the current base URI
  const SerdString new_base_string =
    serd_uri_to_string(env->allocator, new_base_uri);
  if (!new_base_string.data) {
    return SERD_BAD_ALLOC;
  }

  if (zix_string_view_equals(serd_string_view(new_base_string),
                             serd_string_view(env->base_uri_string))) {
    return SERD_NO_CHANGE;
  }

  env->base_uri_string = new_base_string;
  env->base_uri        = serd_parse_uri(env->base_uri_string.data);
  zix_free(env->allocator, old_base_string.data);
  return SERD_SUCCESS;
}

ZIX_PURE_FUNC static SerdPrefix*
serd_env_find(const SerdEnv* const env,
              const char* const    name,
              const size_t         name_len)
{
  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdString prefix_name = env->prefixes[i].name;
    if (prefix_name.length == name_len) {
      if (!memcmp(prefix_name.data, name, name_len)) {
        return &env->prefixes[i];
      }
    }
  }

  return NULL;
}

ZixStringView
serd_env_prefix_uri(const SerdEnv* const env, const ZixStringView name)
{
  if (!env) {
    return zix_empty_string();
  }

  SerdPrefix* const prefix = serd_env_find(env, name.data, name.length);

  return prefix ? zix_substring(prefix->uri.data, prefix->uri.length)
                : zix_empty_string();
}

ZIX_PURE_FUNC static SerdStatus
serd_env_add(SerdEnv* const      env,
             const ZixStringView name,
             const ZixStringView uri)
{
  SerdPrefix* const prefix = serd_env_find(env, name.data, name.length);
  if (prefix) {
    if (!strcmp(prefix->uri.data, uri.data)) {
      return SERD_NO_CHANGE;
    }

    const SerdString uri_string = copy_string_view(env->allocator, uri);
    if (!uri_string.data) {
      return SERD_BAD_ALLOC;
    }

    zix_free(env->allocator, prefix->uri.data);
    prefix->uri = uri_string;
  } else {
    const SerdString  name_string = copy_string_view(env->allocator, name);
    const SerdString  uri_string  = copy_string_view(env->allocator, uri);
    SerdPrefix* const new_prefixes =
      (SerdPrefix*)zix_realloc(env->allocator,
                               env->prefixes,
                               (env->n_prefixes + 1U) * sizeof(SerdPrefix));
    if (!name_string.data || !uri_string.data || !new_prefixes) {
      zix_free(env->allocator, new_prefixes);
      zix_free(env->allocator, uri_string.data);
      zix_free(env->allocator, name_string.data);
      return SERD_BAD_ALLOC;
    }

    new_prefixes[env->n_prefixes].name = name_string;
    new_prefixes[env->n_prefixes].uri  = uri_string;
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
    return serd_env_add(env, name, uri);
  }

  if (!env->base_uri_string.length) {
    return SERD_BAD_ARG; // Unresolvable relative URI
  }

  // Resolve potentially relative URI reference to an absolute URI
  const SerdURIView uri_view     = serd_parse_uri(uri.data);
  const SerdURIView abs_uri_view = serd_resolve_uri(uri_view, env->base_uri);
  const SerdString  abs_uri_string =
    serd_uri_to_string(env->allocator, abs_uri_view);
  if (!abs_uri_string.data) {
    return SERD_BAD_ALLOC;
  }

  assert(serd_uri_string_has_scheme(abs_uri_string.data));

  // Set prefix to resolved (absolute) URI
  const SerdStatus st = serd_env_add(
    env, name, zix_substring(abs_uri_string.data, abs_uri_string.length));
  zix_free(env->allocator, abs_uri_string.data);
  return st;
}

SerdStatus
serd_env_qualify(const SerdEnv* const      env,
                 const ZixStringView       uri,
                 SerdStringPairView* const out)
{
  assert(out);
  if (!env) {
    return SERD_BAD_ARG;
  }

  for (size_t i = 0; i < env->n_prefixes; ++i) {
    const SerdString prefix_uri = env->prefixes[i].uri;
    if (uri.length >= prefix_uri.length) {
      const char* prefix_str = prefix_uri.data;
      const char* uri_str    = uri.data;

      if (!strncmp(uri_str, prefix_str, prefix_uri.length)) {
        out->prefix.data   = env->prefixes[i].name.data;
        out->prefix.length = env->prefixes[i].name.length;
        out->suffix.data   = uri_str + prefix_uri.length;
        out->suffix.length = uri.length - prefix_uri.length;
        return SERD_SUCCESS;
      }
    }
  }

  return SERD_FAILURE;
}

SerdStatus
serd_env_expand(const SerdEnv* const      env,
                const ZixStringView       curie,
                SerdStringPairView* const out)
{
  assert(out);
  if (!env) {
    return SERD_BAD_ARG;
  }

  const char* const str   = curie.data;
  const char* const colon = (const char*)memchr(str, ':', curie.length + 1U);
  if (!colon) {
    return SERD_BAD_CURIE;
  }

  const size_t            name_len = (size_t)(colon - str);
  const SerdPrefix* const prefix   = serd_env_find(env, str, name_len);
  if (prefix) {
    out->prefix.data   = prefix->uri.data;
    out->prefix.length = prefix->uri.length;
    out->suffix.data   = colon + 1U;
    out->suffix.length = curie.length - name_len - 1U;
    return SERD_SUCCESS;
  }

  return SERD_BAD_CURIE;
}

SerdStatus
serd_env_write_prefixes(const SerdEnv* const env, const SerdSink* const sink)
{
  assert(env);
  assert(sink);

  SerdStatus st = SERD_SUCCESS;

  for (size_t i = 0; !st && i < env->n_prefixes; ++i) {
    st = serd_sink_event(
      sink,
      serd_prefix_event(serd_string_view(env->prefixes[i].name),
                        serd_string_view(env->prefixes[i].uri)));
  }

  return st;
}

static SerdStatus
serd_env_on_event(void* const handle, const SerdEvent* const event)
{
  SerdEnv* const env = (SerdEnv*)handle;

  return (event->type == SERD_EVENT_BASE)
           ? serd_env_set_base_uri(env, event->body.uri)
         : (event->type == SERD_EVENT_PREFIX)
           ? serd_env_set_prefix(
               env, event->body.prefix.prefix, event->body.prefix.suffix)
           : SERD_SUCCESS;
}
