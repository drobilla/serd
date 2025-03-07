// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODES_H
#define SERD_NODES_H

#include <serd/attributes.h>
#include <serd/node_args.h>
#include <serd/node_id.h>
#include <serd/object_view.h>
#include <serd/token_view.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_nodes Nodes
   @ingroup serd_storage
   @{
*/

/**
   A set of nodes keyed by integer node ID.

   This stores a set of nodes in memory while hiding implementation details
   about how node memory is actually managed.  This allows for optimizations
   such as sharing strings between nodes or not storing strings for numeric
   nodes at all, while presenting a consistent interface.
*/
typedef struct SerdNodesImpl SerdNodes;

/// Create a new empty set of nodes
SERD_API SerdNodes* ZIX_ALLOCATED
serd_nodes_new(ZixAllocator* ZIX_NULLABLE allocator);

/// Free `nodes` and all nodes that are stored in it
SERD_API void
serd_nodes_free(SerdNodes* ZIX_NULLABLE nodes);

/**
   Return the number of interned nodes.

   Note that adding a new node may increase this count by more than one, since
   datatypes and languages are stored internally as separate nodes.
*/
SERD_PURE_API size_t
serd_nodes_size(const SerdNodes* ZIX_NONNULL nodes);

/**
   Find an existing ID for a node or allocate a new one.

   @return The ID for the given node, or zero on error.
*/
SERD_API SerdNodeID
serd_nodes_id(SerdNodes* ZIX_NONNULL nodes, SerdNodeArgs args);

/**
   Find an existing ID for a node.

   @return The ID for the given node, or zero if it isn't present.
*/
SERD_API SerdNodeID
serd_nodes_existing_id(const SerdNodes* ZIX_NONNULL nodes, SerdNodeArgs args);

/**
   Return a view of a stored token node.

   @return A view of the given node, or an empty view with type #SERD_NOTHING
   if it isn't found.
*/
SERD_PURE_API SerdTokenView
serd_nodes_get_token(const SerdNodes* ZIX_NONNULL nodes, SerdNodeID id);

/**
   Return a view of a stored object node.

   @return A view of the given node, or an empty view with type #SERD_NOTHING
   if it isn't found.
*/
SERD_PURE_API SerdObjectView
serd_nodes_get_object(const SerdNodes* ZIX_NONNULL nodes, SerdNodeID id);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODES_H
