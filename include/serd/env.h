// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

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
SERD_API
SerdEnv* SERD_ALLOCATED
serd_env_new(SerdWorld* SERD_NONNULL world, ZixStringView base_uri);

/// Copy an environment
SERD_API
SerdEnv* SERD_ALLOCATED
serd_env_copy(ZixAllocator* SERD_NULLABLE  allocator,
              const SerdEnv* SERD_NULLABLE env);

/// Return true iff `a` is equal to `b`
SERD_PURE_API
bool
serd_env_equals(const SerdEnv* SERD_NULLABLE a, const SerdEnv* SERD_NULLABLE b);

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
serd_env_set_base_uri(SerdEnv* SERD_NONNULL env, ZixStringView uri);

/**
   Set the current base URI to a filesystem path.

   This will set the base URI to a properly formatted file URI that points to
   the canonical version of `path`.  Note that this requires the path to
   actually exist.
*/
SERD_API
SerdStatus
serd_env_set_base_path(SerdEnv* SERD_NONNULL env, ZixStringView path);

/**
   Set a namespace prefix.

   A namespace prefix is used to expand CURIE nodes, for example, with the
   prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal" will
   expand to "http://www.w3.org/2001/XMLSchema#decimal".
*/
SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv* SERD_NONNULL env,
                    ZixStringView         name,
                    ZixStringView         uri);

/**
   Qualify `uri` into a prefix and suffix (like a CURIE) if possible.

   @param env Environment with prefixes to use.

   @param uri URI to qualify.

   @param prefix On success, pointed to a prefix string slice, which is only
   valid until the next time `env` is mutated.

   @param suffix On success, pointed to a suffix string slice, which is only
   valid until the next time `env` is mutated.

   @return #SERD_SUCCESS, or #SERD_FAILURE if `uri` can not be qualified with
   `env`.
*/
SERD_API
SerdStatus
serd_env_qualify(const SerdEnv* SERD_NULLABLE env,
                 ZixStringView                uri,
                 ZixStringView* SERD_NONNULL  prefix,
                 ZixStringView* SERD_NONNULL  suffix);

/**
   Expand `curie` to an absolute URI if possible.

   For example, if `env` has the prefix "rdf" set to
   <http://www.w3.org/1999/02/22-rdf-syntax-ns#>, then calling this with curie
   "rdf:type" will produce <http://www.w3.org/1999/02/22-rdf-syntax-ns#type>.

   Returns null if `node` can not be expanded.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_env_expand_curie(const SerdEnv* SERD_NULLABLE env, ZixStringView curie);

/**
   Expand `node` to an absolute URI if possible.

   Returns null if `node` can not be expanded.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_env_expand_node(const SerdEnv* SERD_NULLABLE  env,
                     const SerdNode* SERD_NULLABLE node);

/// Write all prefixes in `env` to `sink`
SERD_API
SerdStatus
serd_env_write_prefixes(const SerdEnv* SERD_NONNULL  env,
                        const SerdSink* SERD_NONNULL sink);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ENV_H
