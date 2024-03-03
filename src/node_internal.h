// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_INTERNAL_H
#define SERD_SRC_NODE_INTERNAL_H

#include "serd/node.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>
#include <stdint.h>

static const size_t serd_node_align = 2 * sizeof(uint32_t);

/// Return the total size in bytes of a node with the given string length
ZIX_CONST_FUNC size_t
serd_node_size_for_length(size_t length);

/// Return the total size in bytes of a node
ZIX_PURE_FUNC size_t
serd_node_total_size(const SerdNode* ZIX_NONNULL node);

/// Return a mutable pointer to the string buffer of a node
ZIX_CONST_FUNC char* ZIX_NONNULL
serd_node_buffer(SerdNode* ZIX_NONNULL node);

/// Return a pointer to the accompanying meta node of a node
ZIX_PURE_FUNC SerdNode* ZIX_NONNULL
serd_node_meta(SerdNode* ZIX_NONNULL node);

/// Allocate a new node with a given total size (including header and padding)
ZIX_MALLOC_FUNC SerdNode* ZIX_ALLOCATED
serd_node_malloc(ZixAllocator* ZIX_NULLABLE allocator, size_t size);

/// Set `dst` to be equal to `src`, re-allocating it if necessary
SerdStatus
serd_node_set(ZixAllocator* ZIX_NULLABLE         allocator,
              SerdNode* ZIX_NONNULL* ZIX_NONNULL dst,
              const SerdNode* ZIX_NONNULL        src);

/// Set a flag on a node, leaving others as they were
void
serd_node_set_flag(SerdNode* ZIX_NONNULL node, SerdNodeFlag flag);

/// Set the header of a node, completely overwriting previous values
void
serd_node_set_header(SerdNode* ZIX_NONNULL node,
                     size_t                length,
                     SerdNodeFlags         flags,
                     SerdNodeType          type);

/// Zero the padding (at least one trailing null byte) of a node
void
serd_node_zero_pad(SerdNode* ZIX_NONNULL node);

#endif // SERD_SRC_NODE_INTERNAL_H
