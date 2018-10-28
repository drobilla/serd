// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/*
  Nothing here is actually used outside the nodes implementation, but we need
  the types to be defined before including zix/hash.h to enable its type-safe
  interface.  Putting those here is a way of doing that without messy hacks
  like including headers half-way through the implementation.
*/

#ifndef SERD_SRC_NODES_H
#define SERD_SRC_NODES_H

#include "node.h"

#include "serd/node.h"

#include <stddef.h>

/**
   The header of an entry in the nodes table.

   The table stores nodes with an additional reference count.  For efficiency,
   entries are allocated as a single block of memory, which start with this
   header and are followed by the body of the node.

   This structure must be the same size as SerdNode to preserve the alignment
   of the contained node.  This is a bit wasteful, but the alignment guarantee
   allows the node implementation to avoid messy casts and byte-based pointer
   arithmetic that could cause alignment problems.  This might be worth
   reconsidering, since this wasted space has a real (if small) negative
   impact, while the alignment guarantee just allows the implementation to use
   stricter compiler settings.

   Better yet, shrink SerdNode down to size_t, which is malloc's alignment
   guarantee, and all of this goes away, at the cost of a reduced maximum
   length for literal nodes.
*/
typedef struct {
  size_t   refs; ///< Reference count
  unsigned pad1; ///< Padding to align the following SerdNode
  unsigned pad2; ///< Padding to align the following SerdNode
} NodesEntryHead;

/**
   An entry in the nodes table.

   This is a variably-sized structure that is allocated specifically to contain
   the node.
*/
typedef struct {
  NodesEntryHead head; ///< Extra data associated with the node
  SerdNode       node; ///< Node header (body immediately follows)
} NodesEntry;

#endif // SERD_SRC_NODES_H
