// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODES_H
#define SERD_NODES_H

#include <serd/attributes.h>
#include <serd/node_args.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_nodes Nodes
   @ingroup serd_storage
   @{
*/

/**
   A set of nodes keyed by integer node ID.

   This stores a set of nodes with efficient access via integer ID.  The public
   interface is intentionally simple in order to isolate client code from
   storage details.
*/
typedef struct SerdNodesImpl SerdNodes;

/// Create a new empty set of nodes
SERD_API SerdNodes* ZIX_ALLOCATED
serd_nodes_new(ZixAllocator* ZIX_NULLABLE allocator);

/// Free a set of nodes and all its contents
SERD_API void
serd_nodes_free(SerdNodes* ZIX_NULLABLE nodes);

/**
   Find or allocate an ID for a node.

   @param nodes Nodes to modify.
   @param args Description of the node to find or create.
   @return The ID for the given node, or zero on error.
*/
SERD_API SerdNodeID
serd_nodes_id(SerdNodes* ZIX_NONNULL nodes, SerdNodeArgs args);

/**
   Find an existing ID for a node.

   @param nodes Nodes to search.
   @param args Description of the node to search for.
   @return The ID of an equivalent node, or zero.
*/
SERD_API SerdNodeID
serd_nodes_find(const SerdNodes* ZIX_NONNULL nodes, SerdNodeArgs args);

/**
   Find or allocate an ID for a node from another set.

   @param nodes Nodes to modify.
   @param source Nodes to copy from.
   @param id The ID of the node in `source` to copy.
   @return The ID of the equivalent node in `nodes`, or zero.
*/
SERD_API SerdNodeID
serd_nodes_crib(SerdNodes* ZIX_NONNULL       nodes,
                const SerdNodes* ZIX_NONNULL source,
                SerdNodeID                   id);

/**
   Return the type of a stored node.

   @param nodes Nodes to search.
   @param id ID of the node to inspect.
   @return The type of the stored node, or #SERD_NOTHING.
*/
SERD_PURE_API SerdNodeType
serd_nodes_type(const SerdNodes* ZIX_NONNULL nodes, SerdNodeID id);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODES_H
