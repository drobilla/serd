// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include <serd/attributes.h>
#include <serd/status.h>
#include <serd/uri.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_env Environment
   @ingroup serd_streaming
   @{
*/

/**
   Lexical environment for URI references.

   An "env" is a context where relative URI references can be resolved, and
   CURIEs expanded, into full URIs.  It has a base URI and a set of namespace
   prefixes, with a low-level API that doesn't require allocation except for
   modications to the env itself.
*/
typedef struct SerdEnvImpl SerdEnv;

/// Create a new environment
SERD_API SerdEnv* ZIX_ALLOCATED
serd_env_new(ZixAllocator* ZIX_NULLABLE allocator, ZixStringView base_uri);

/// Free an environment allocated by #serd_env_new
SERD_API void
serd_env_free(SerdEnv* ZIX_NULLABLE env);

/**
   Set the current base URI.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `uri` is relative but a base URI
   isn't set, or #SERD_BAD_ALLOC.
*/
SERD_API SerdStatus
serd_env_set_base_uri(SerdEnv* ZIX_NONNULL env, ZixStringView uri);

/**
   Set a namespace prefix.

   If a prefix with the given name is already set, the old value is overridden.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `uri` is relative but no base URI is
   set, or #SERD_BAD_ALLOC.
*/
SERD_API SerdStatus
serd_env_set_prefix(SerdEnv* ZIX_NONNULL env,
                    ZixStringView        name,
                    ZixStringView        uri);

/**
   Return a view of the current base URI string.

   @return A view of an absolute base URI, or the empty string.
*/
SERD_PURE_API ZixStringView
serd_env_base_uri_string(const SerdEnv* ZIX_NULLABLE env);

/**
   Return a parsed view of the current base URI.

   @return A parsed absolute base URI, or a null URI.
*/
SERD_PURE_API SerdURIView
serd_env_base_uri_view(const SerdEnv* ZIX_NULLABLE env);

/**
   Get the URI for a namespace prefix.

   @return A view of a URI string (valid until `env` is mutated), or the empty
   string if the prefix wasn't found.
*/
SERD_PURE_API ZixStringView
serd_env_prefix_uri(const SerdEnv* ZIX_NULLABLE env, ZixStringView name);

/**
   Qualify `uri` into a prefix name and suffix if possible.

   @param env Environment with prefixes to qualify against.

   @param uri URI to qualify.

   @param[out] prefix On success, pointed to a prefix name which is valid until
   `env` is mutated.

   @param[out] suffix On success, pointed to a suffix of `uri`.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `env` is null, or #SERD_FAILURE if
   `uri` can't be qualified.
*/
SERD_API SerdStatus
serd_env_qualify(const SerdEnv* ZIX_NULLABLE env,
                 ZixStringView               uri,
                 ZixStringView* ZIX_NONNULL  prefix,
                 ZixStringView* ZIX_NONNULL  suffix);

/**
   Expand `curie` into a URI prefix and suffix if possible.

   @param env Environment with prefixes to expand with.

   @param curie CURIE to expand into a URI.

   @param[out] prefix On success, pointed to a URI prefix which is valid until
   `env` is mutated.

   @param[out] suffix On success, pointed to a suffix of `curie`.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `env` is null, or #SERD_BAD_CURIE if
   the prefix wasn't found.
*/
SERD_API SerdStatus
serd_env_expand(const SerdEnv* ZIX_NULLABLE env,
                ZixStringView               curie,
                ZixStringView* ZIX_NONNULL  prefix,
                ZixStringView* ZIX_NONNULL  suffix);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ENV_H
