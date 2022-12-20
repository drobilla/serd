// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/uri.h"
#include "zix/attributes.h"

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
serd_env_new(const SerdNode* ZIX_NULLABLE base_uri);

/// Free `env`
SERD_API void
serd_env_free(SerdEnv* ZIX_NULLABLE env);

/// Get the current base URI
SERD_API const SerdNode* ZIX_NONNULL
serd_env_base_uri(const SerdEnv* ZIX_NONNULL env,
                  SerdURIView* ZIX_NULLABLE  out);

/// Set the current base URI
SERD_API SerdStatus
serd_env_set_base_uri(SerdEnv* ZIX_NONNULL         env,
                      const SerdNode* ZIX_NULLABLE uri);

/**
   Set a namespace prefix.

   A namespace prefix is used to expand CURIE nodes, for example, with the
   prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal" will
   expand to "http://www.w3.org/2001/XMLSchema#decimal".
*/
SERD_API SerdStatus
serd_env_set_prefix(SerdEnv* ZIX_NONNULL        env,
                    const SerdNode* ZIX_NONNULL name,
                    const SerdNode* ZIX_NONNULL uri);

/// Set a namespace prefix
SERD_API SerdStatus
serd_env_set_prefix_from_strings(SerdEnv* ZIX_NONNULL    env,
                                 const char* ZIX_NONNULL name,
                                 const char* ZIX_NONNULL uri);

/// Qualify `uri` into a CURIE if possible
SERD_API bool
serd_env_qualify(const SerdEnv* ZIX_NULLABLE env,
                 const SerdNode* ZIX_NONNULL uri,
                 SerdNode* ZIX_NONNULL       prefix,
                 SerdStringView* ZIX_NONNULL suffix);

/**
   Expand `curie`.

   Errors: SERD_BAD_ARG if `curie` is not valid, or SERD_BAD_CURIE if prefix is
   not defined in `env`.
*/
SERD_API SerdStatus
serd_env_expand(const SerdEnv* ZIX_NULLABLE env,
                const SerdNode* ZIX_NONNULL curie,
                SerdStringView* ZIX_NONNULL uri_prefix,
                SerdStringView* ZIX_NONNULL uri_suffix);

/**
   Expand `node`, which must be a CURIE or URI, to a full URI.

   Returns null if `node` can not be expanded.
*/
SERD_API SerdNode
serd_env_expand_node(const SerdEnv* ZIX_NULLABLE env,
                     const SerdNode* ZIX_NONNULL node);

/// Call `func` for each prefix defined in `env`
SERD_API void
serd_env_foreach(const SerdEnv* ZIX_NONNULL env,
                 SerdPrefixFunc ZIX_NONNULL func,
                 void* ZIX_UNSPECIFIED      handle);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ENV_H
