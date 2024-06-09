// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODES_H
#define SERD_NODES_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_nodes Nodes
   @ingroup serd_storage
   @{
*/

/// Hashing node container for interning and simplified memory management
typedef struct SerdNodesImpl SerdNodes;

/// Create a new node set
SERD_API SerdNodes* ZIX_ALLOCATED
serd_nodes_new(ZixAllocator* ZIX_NULLABLE allocator);

/**
   Free `nodes` and all nodes that are stored in it.

   Note that this invalidates any node pointers previously returned from
   `nodes`.
*/
SERD_API void
serd_nodes_free(SerdNodes* ZIX_NULLABLE nodes);

/// Return the number of interned nodes
SERD_PURE_API size_t
serd_nodes_size(const SerdNodes* ZIX_NONNULL nodes);

/**
   Return the existing interned copy of a node if it exists.

   This either returns an equivalent to the given node, or null if this node
   has not been interned.
*/
SERD_API const SerdNode* ZIX_NULLABLE
serd_nodes_existing(const SerdNodes* ZIX_NONNULL nodes,
                    const SerdNode* ZIX_NULLABLE node);

/**
   Intern `node`.

   Multiple calls with equivalent nodes will return the same pointer.

   @return A node that is different than, but equivalent to, `node`.
*/
SERD_API const SerdNode* ZIX_ALLOCATED
serd_nodes_intern(SerdNodes* ZIX_NONNULL       nodes,
                  const SerdNode* ZIX_NULLABLE node);

/**
   Make a node of any type.

   A new node will be added if an equivalent node is not already in the set.
*/
SERD_API const SerdNode* ZIX_ALLOCATED
serd_nodes_get(SerdNodes* ZIX_NONNULL nodes, SerdNodeArgs args);

/**
   Dereference `node`.

   Decrements the reference count of `node`, and frees the internally stored
   equivalent node if this was the last reference.  Does nothing if no node
   equivalent to `node` is stored in `nodes`.
*/
SERD_API void
serd_nodes_deref(SerdNodes* ZIX_NONNULL       nodes,
                 const SerdNode* ZIX_NULLABLE node);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODES_H
