// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_WORLD_H
#define SERD_WORLD_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/nodes.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_world World
   @ingroup serd_library
   @{
*/

/// Global library state
typedef struct SerdWorldImpl SerdWorld;

/**
   Create a new Serd World.

   It is safe to use multiple worlds in one process, though no objects can be
   shared between worlds.
*/
SERD_MALLOC_API
SerdWorld* SERD_ALLOCATED
serd_world_new(SerdAllocator* SERD_NULLABLE allocator);

/// Free `world`
SERD_API
void
serd_world_free(SerdWorld* SERD_NULLABLE world);

/// Return the allocator used by `world`
SERD_PURE_API
SerdAllocator* SERD_NONNULL
serd_world_allocator(const SerdWorld* SERD_NONNULL world);

/**
   Return the nodes cache in `world`.

   The returned cache is owned by the world and contains various nodes used
   frequently by the implementation.  For convenience, it may be used to store
   additional nodes which will be freed when the world is freed.
*/
SERD_PURE_API
SerdNodes* SERD_NONNULL
serd_world_nodes(SerdWorld* SERD_NONNULL world);

/**
   Return a unique blank node.

   The returned node is valid only until the next time serd_world_get_blank()
   is called or the world is destroyed.
*/
SERD_API
const SerdNode* SERD_NONNULL
serd_world_get_blank(SerdWorld* SERD_NONNULL world);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_WORLD_H
