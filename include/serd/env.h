// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include "serd/attributes.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_env Environment
   @ingroup serd_streaming
   @{
*/

/// Lexical environment for resolving URI references
typedef struct SerdEnvImpl SerdEnv;

/// Create a new environment
SERD_API SerdEnv* ZIX_ALLOCATED
serd_env_new(ZixStringView base_uri);

/// Copy an environment
SERD_API SerdEnv* ZIX_ALLOCATED
serd_env_copy(const SerdEnv* ZIX_NULLABLE env);

/// Return true iff `a` is equal to `b`
SERD_PURE_API bool
serd_env_equals(const SerdEnv* ZIX_NULLABLE a, const SerdEnv* ZIX_NULLABLE b);

/// Free `env`
SERD_API void
serd_env_free(SerdEnv* ZIX_NULLABLE env);

/// Return a sink interface that updates `env` on base URI or prefix changes
SERD_CONST_API const SerdSink* ZIX_NONNULL
serd_env_sink(SerdEnv* ZIX_NONNULL env);

/// Get a view of the current base URI string
SERD_PURE_API ZixStringView
serd_env_base_uri_string(const SerdEnv* ZIX_NULLABLE env);

/// Get a parsed view of the current base URI
SERD_PURE_API SerdURIView
serd_env_base_uri_view(const SerdEnv* ZIX_NULLABLE env);

/// Set the current base URI
SERD_API SerdStatus
serd_env_set_base_uri(SerdEnv* ZIX_NONNULL env, ZixStringView uri);

/**
   Set a namespace prefix.

   A namespace prefix is used to expand CURIE nodes, for example, with the
   prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal" will
   expand to "http://www.w3.org/2001/XMLSchema#decimal".
*/
SERD_API SerdStatus
serd_env_set_prefix(SerdEnv* ZIX_NONNULL env,
                    ZixStringView        name,
                    ZixStringView        uri);

/**
   Get the URI for a namespace prefix.

   @return A view of a string owned by `env` which is valid until the next time
   `env` is muted, or length 0 if no such prefix is defined.
*/
SERD_PURE_API ZixStringView
serd_env_get_prefix(const SerdEnv* ZIX_NULLABLE env, ZixStringView name);

/**
   Qualify `uri` into a prefix and suffix (like a CURIE) if possible.

   This function searches for a matching prefix, but never allocates any
   memory.  The output parameters will point to internal strings owned by
   `env`, or the `uri` parameter.

   @param env Environment with prefixes to use.

   @param uri URI to qualify.

   @param[out] prefix On success, mutated to point to a prefix name which is
   valid until the next time `env` is mutated.

   @param[out] suffix On success, mutated to point to a URI suffix which is
   valid until the next time `env` is mutated.

   @return #SERD_SUCCESS, or #SERD_FAILURE if `uri` can not be qualified with
   `env`.
*/
SERD_API SerdStatus
serd_env_qualify(const SerdEnv* ZIX_NULLABLE env,
                 ZixStringView               uri,
                 ZixStringView* ZIX_NONNULL  prefix,
                 ZixStringView* ZIX_NONNULL  suffix);

/**
   Expand `curie` into a URI prefix and suffix if possible.

   This function looks up the prefix, but never allocates any memory.  The
   output parameters will point to internal strings owned by `env`, or the
   `curie` parameter.

   @param env Environment with prefixes to use.

   @param curie CURIE to expand into a URI.

   @param[out] prefix On success, mutated to point to a URI prefix which is
   valid until the next time `env` is mutated.

   @param[out] suffix On success, mutated to point to a URI suffix which is
   valid until the next time `env` is mutated.

   @return #SERD_SUCCESS, or #SERD_BAD_CURIE if the prefix wasn't found.
   `env`.
*/
SERD_API SerdStatus
serd_env_expand(const SerdEnv* ZIX_NULLABLE env,
                ZixStringView               curie,
                ZixStringView* ZIX_NONNULL  prefix,
                ZixStringView* ZIX_NONNULL  suffix);

/**
   Describe an environment to a sink.

   This will send to `sink` an event for the base URI (if one is defined),
   followed by events for any defined namespace prefixes.
*/
SERD_API SerdStatus
serd_env_describe(const SerdEnv* ZIX_NONNULL  env,
                  const SerdSink* ZIX_NONNULL sink);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ENV_H
