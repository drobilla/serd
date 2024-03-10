// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_INTERNAL_H
#define SERD_SRC_NODE_INTERNAL_H

#include "serd/node.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>

/// Return a string length padded to the number used with termination in a node
ZIX_CONST_FUNC size_t
serd_node_pad_length(size_t n_bytes);

/// Return the total size in bytes of a node
ZIX_PURE_FUNC size_t
serd_node_total_size(const SerdNode* ZIX_NONNULL node);

/// Return a mutable pointer to the string buffer of a node
ZIX_CONST_FUNC char* ZIX_NONNULL
serd_node_buffer(SerdNode* ZIX_NONNULL node);

/// Return a pointer to the "meta" node for a node (datatype or language)
ZIX_PURE_FUNC const SerdNode* ZIX_UNSPECIFIED
serd_node_meta(const SerdNode* ZIX_NONNULL node);

/// Allocate a new node with a given maximum string length
SerdNode* ZIX_ALLOCATED
serd_node_malloc(ZixAllocator* ZIX_NULLABLE allocator, size_t max_length);

/// Set the header of a node, completely overwriting previous values
void
serd_node_set_header(SerdNode* ZIX_NONNULL node,
                     size_t                length,
                     SerdNodeFlags         flags,
                     SerdNodeType          type);

/// Set `dst` to be equal to `src`, re-allocating it if necessary
SerdStatus
serd_node_set(ZixAllocator* ZIX_NULLABLE         allocator,
              SerdNode* ZIX_NONNULL* ZIX_NONNULL dst,
              const SerdNode* ZIX_NONNULL        src);

#endif // SERD_SRC_NODE_INTERNAL_H
