// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/string_view.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_env Environment
   @ingroup serd
   @{
*/

/// Lexical environment for relative URIs or CURIEs (base URI and namespaces)
typedef struct SerdEnvImpl SerdEnv;

/// Create a new environment
SERD_API
SerdEnv* SERD_ALLOCATED
serd_env_new(SerdStringView base_uri);

/// Free `env`
SERD_API
void
serd_env_free(SerdEnv* SERD_NULLABLE env);

/// Get the current base URI
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_env_base_uri(const SerdEnv* SERD_NULLABLE env);

/// Set the current base URI
SERD_API
SerdStatus
serd_env_set_base_uri(SerdEnv* SERD_NONNULL env, SerdStringView uri);

/**
   Set a namespace prefix.

   A namespace prefix is used to expand CURIE nodes, for example, with the
   prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal" will
   expand to "http://www.w3.org/2001/XMLSchema#decimal".
*/
SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv* SERD_NONNULL env,
                    SerdStringView        name,
                    SerdStringView        uri);

/// Qualify `uri` into a CURIE if possible
SERD_API
bool
serd_env_qualify(const SerdEnv* SERD_NULLABLE                env,
                 const SerdNode* SERD_NONNULL                uri,
                 const SerdNode* SERD_NULLABLE* SERD_NONNULL prefix,
                 SerdStringView* SERD_NONNULL                suffix);

/**
   Expand `curie`.

   Errors: SERD_ERR_BAD_ARG if `curie` is not valid, or SERD_ERR_BAD_CURIE if
   prefix is not defined in `env`.
*/
SERD_API
SerdStatus
serd_env_expand(const SerdEnv* SERD_NULLABLE  env,
                const SerdNode* SERD_NULLABLE curie,
                SerdStringView* SERD_NONNULL  uri_prefix,
                SerdStringView* SERD_NONNULL  uri_suffix);

/**
   Expand `node`, which must be a CURIE or URI, to a full URI.

   Returns null if `node` can not be expanded.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_env_expand_node(const SerdEnv* SERD_NULLABLE env,
                     const SerdNode* SERD_NONNULL node);

/// Call `func` for each prefix defined in `env`
SERD_API
void
serd_env_foreach(const SerdEnv* SERD_NONNULL env,
                 SerdPrefixFunc SERD_NONNULL func,
                 void* SERD_NULLABLE         handle);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ENV_H
