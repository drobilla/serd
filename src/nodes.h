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

#include "node_impl.h"

#include "serd/node.h"

#include <stddef.h>

/**
   The header of an entry in the nodes table.

   The table stores nodes with an additional reference count.  For efficiency,
   entries are allocated as a single block of memory, which start with this
   header and are followed by the body of the node.
*/
typedef struct {
  size_t   refs; ///< Reference count
  // FIXME: Size must be the same as SerdNode
  size_t   pad1;
  unsigned pad2;
  unsigned pad3;
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
