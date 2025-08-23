// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include <serd/attributes.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <serd/string_pair_view.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stdbool.h>

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

/// Return a sink interface that updates `env` on base URI or prefix changes
SERD_CONST_API const SerdSink* ZIX_NONNULL
serd_env_sink(SerdEnv* ZIX_NONNULL env);

/**
   Set the current base URI.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `uri` is relative but a base URI
   isn't set, or #SERD_BAD_ALLOC.
*/
SERD_API SerdStatus
serd_env_set_base_uri(SerdEnv* ZIX_NONNULL env, ZixStringView uri);

/**
   Set the current base URI from a filesystem path.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `uri` is relative but a base URI
   isn't set, or #SERD_BAD_ALLOC.
*/
SERD_API SerdStatus
serd_env_set_base_path(SerdEnv* ZIX_NONNULL env, ZixStringView path);

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

   @param[out] out On success, points to a prefix name in `env` and a suffix of
   `uri`.  The prefix is valid until `env` is mutated.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `env` is null, or #SERD_FAILURE if
   `uri` can't be qualified.
*/
SERD_API SerdStatus
serd_env_qualify(const SerdEnv* ZIX_NULLABLE     env,
                 ZixStringView                   uri,
                 SerdStringPairView* ZIX_NONNULL out);

/**
   Expand `curie` into a URI prefix and suffix if possible.

   @param env Environment with prefixes to expand with.

   @param curie CURIE to expand into a URI.

   @param[out] out On success, points to a URI prefix and suffix in `env` and
   `curie`.  The prefix is valid until `env` is mutated.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `env` is null, or #SERD_BAD_CURIE if
   the prefix wasn't found.
*/
SERD_API SerdStatus
serd_env_expand(const SerdEnv* ZIX_NULLABLE     env,
                ZixStringView                   curie,
                SerdStringPairView* ZIX_NONNULL out);

/**
   Resolve `token` into a URI prefix and suffix if possible.

   This can expand both CURIEs and relative URI references.  Note that this can
   only do simple resolution by expanding a prefix or joining with a base URI.
   For full URI resolution, see #serd_resolve_uri.

   @param env Environment with prefixes to expand with.

   @param token URI or CURIE token expand into a full URI.

   @param[out] out On success, points to a URI prefix and suffix in `env` and
   `token`.  The prefix is valid until `env` is mutated.

   @return #SERD_SUCCESS, #SERD_BAD_ARG if `env` is null or the type of `token`
   is unsupported, #SERD_BAD_URI if `token` is a URI that can't be resolved, or
   #SERD_BAD_CURIE if `token` is a CURIE that can't be expanded.
*/
SERD_API SerdStatus
serd_env_resolve(const SerdEnv* ZIX_NULLABLE     env,
                 SerdTokenView                   token,
                 SerdStringPairView* ZIX_NONNULL out);

/**
   Write the prefixes in an environment as events to a sink.

   Writing stops if any error is encountered.

   @param env Environment with prefixes to write.
   @param sink Sink for #SERD_EVENT_PREFIX events.
   @return The last status returned by the sink's event function.
*/
SERD_API SerdStatus
serd_env_write_prefixes(const SerdEnv* ZIX_NONNULL  env,
                        const SerdSink* ZIX_NONNULL sink);

/**
   Return whether two tokens are equal within an environment.

   This expands tokens if necessary, so CURIEs and relative URI references will
   compare equal to their absolute counterparts.

   @param env Environment to compare nodes within.
   @param lhs Left-hand-side token to compare.
   @param rhs Right-hand-side token to compare.
   @return Whether `lhs` equals `rhs` when both are expanded.
*/
SERD_API bool
serd_env_tokens_equal(const SerdEnv* ZIX_NONNULL env,
                      SerdTokenView              lhs,
                      SerdTokenView              rhs);

/**
   Return whether two objects are equal within an environment.

   This expands tokens if necessary, so CURIEs and relative URI references
   (including datatypes) will compare equal to their absolute counterparts.

   @param env Environment to compare nodes within.
   @param lhs Left-hand-side object to compare.
   @param rhs Right-hand-side object to compare.
   @return Whether `lhs` equals `rhs` when both are expanded.
*/
SERD_API bool
serd_env_objects_equal(const SerdEnv* ZIX_NONNULL env,
                       SerdObjectView             lhs,
                       SerdObjectView             rhs);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ENV_H
