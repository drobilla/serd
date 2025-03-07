// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODES_INTERNAL_H
#define SERD_SRC_NODES_INTERNAL_H

#include <serd/attributes.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/token_view.h>
#include <zix/attributes.h>

#include <stdbool.h>

/**
   Compare two stored nodes.

   @param nodes Nodes to compare within.
   @param lhs ID of the node on the left-hand side.
   @param rhs ID of the node on the right-hand side.

   @return Less than, equal to, or greater than zero if the left node is less
   than, equal to, or greater than the right node, respectively.
*/
SERD_PURE_API int
serd_nodes_compare(const SerdNodes* ZIX_NONNULL nodes,
                   SerdNodeID                   lhs,
                   SerdNodeID                   rhs);

/**
   Return whether a stored token equals one from another set.

   @param nodes The first set of nodes.
   @param id ID of the node in the first set.
   @param other The second set of nodes.
   @param other_id ID of the node in the second set.

   @return Less than, equal to, or greater than zero if the left node is less
   than, equal to, or greater than the right node, respectively.
*/
SERD_PURE_API bool
serd_nodes_equals_foreign_token(const SerdNodes* ZIX_NONNULL nodes,
                                SerdNodeID                   id,
                                const SerdNodes* ZIX_NONNULL other,
                                SerdNodeID                   other_id);

/**
   Return whether a stored node equals one from another set.

   @param nodes The first set of nodes.
   @param id ID of the node in the first set.
   @param other The second set of nodes.
   @param other_id ID of the node in the second set.

   @return Less than, equal to, or greater than zero if the left node is less
   than, equal to, or greater than the right node, respectively.
*/
SERD_PURE_API bool
serd_nodes_equals_foreign_object(const SerdNodes* ZIX_NONNULL nodes,
                                 SerdNodeID                   id,
                                 const SerdNodes* ZIX_NONNULL other,
                                 SerdNodeID                   other_id);

/**
   Return a view of a stored token node.

   @param nodes Nodes to search.
   @param id ID of the node to view.

   @return A view of the given node, or an empty view with type #SERD_NOTHING
   if it isn't found.
*/
SERD_PURE_API SerdTokenView
serd_nodes_get_token(const SerdNodes* ZIX_NONNULL nodes, SerdNodeID id);

/**
   Return a view of a stored object node.

   @param nodes Nodes to search.
   @param id ID of the node to view.

   @return A view of the given node, or an empty view with type #SERD_NOTHING
   if it isn't found.
*/
SERD_PURE_API SerdObjectView
serd_nodes_get_object(const SerdNodes* ZIX_NONNULL nodes, SerdNodeID id);

/**
   Return a view of a stored literal node.

   @param nodes Nodes to search.
   @param id ID of the node to view.

   @return A view of the given node, or an empty view if it isn't found.
*/
SERD_PURE_API SerdLiteralView
serd_nodes_get_literal(const SerdNodes* ZIX_NONNULL nodes, SerdNodeID id);

#endif // SERD_NODES_INTERNAL_H
