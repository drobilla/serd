// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "zix/attributes.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_env Environment
   @ingroup serd_streaming
   @{
*/

/// Lexical environment for relative URIs or CURIEs (base URI and namespaces)
typedef struct SerdEnvImpl SerdEnv;

/// Create a new environment
SERD_API SerdEnv* ZIX_ALLOCATED
serd_env_new(SerdStringView base_uri);

/// Copy an environment
SERD_API SerdEnv* ZIX_ALLOCATED
serd_env_copy(const SerdEnv* ZIX_NULLABLE env);

/// Return true iff `a` is equal to `b`
SERD_PURE_API bool
serd_env_equals(const SerdEnv* ZIX_NULLABLE a, const SerdEnv* ZIX_NULLABLE b);

/// Free `env`
SERD_API void
serd_env_free(SerdEnv* ZIX_NULLABLE env);

/// Get the current base URI
SERD_PURE_API const SerdNode* ZIX_NULLABLE
serd_env_base_uri(const SerdEnv* ZIX_NULLABLE env);

/// Set the current base URI
SERD_API SerdStatus
serd_env_set_base_uri(SerdEnv* ZIX_NONNULL env, SerdStringView uri);

/**
   Set a namespace prefix.

   A namespace prefix is used to expand CURIE nodes, for example, with the
   prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal" will
   expand to "http://www.w3.org/2001/XMLSchema#decimal".
*/
SERD_API SerdStatus
serd_env_set_prefix(SerdEnv* ZIX_NONNULL env,
                    SerdStringView       name,
                    SerdStringView       uri);

/**
   Qualify `uri` into a CURIE if possible.

   Returns null if `uri` can not be qualified (usually because no corresponding
   prefix is defined).
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_env_qualify(const SerdEnv* ZIX_NULLABLE  env,
                 const SerdNode* ZIX_NULLABLE uri);

/**
   Expand `node` to an absolute URI if possible.

   Returns null if `node` can not be expanded.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_env_expand_node(const SerdEnv* ZIX_NULLABLE  env,
                     const SerdNode* ZIX_NULLABLE node);

/// Write all prefixes in `env` to `sink`
SERD_API SerdStatus
serd_env_write_prefixes(const SerdEnv* ZIX_NONNULL  env,
                        const SerdSink* ZIX_NONNULL sink);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ENV_H
