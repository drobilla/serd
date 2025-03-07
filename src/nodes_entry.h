// Copyright 2021-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/*
  These definitions are only used internally by nodes.c, but are here so this
  header can be included before zix/hash.h, allowing its type-safe interface.
  It's either this, or having includes after definitions.
*/

#ifndef SERD_SRC_NODES_ENTRY_H
#define SERD_SRC_NODES_ENTRY_H

#include "node.h"
#include "node_impl.h" // IWYU pragma: keep

#include <serd/node_id.h>

/// The header of an entry in a nodes collection
typedef struct {
  SerdNodeID id;
} NodesEntryHead;

/// An entry in a nodes collection
typedef struct {
  NodesEntryHead head; ///< Extra data associated with the node
  SerdNode       node; ///< Node header (body immediately follows)
} NodesEntry;

#endif // SERD_SRC_NODES_ENTRY_H
