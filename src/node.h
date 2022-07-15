// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_H
#define SERD_SRC_NODE_H

#include "exess/exess.h"
#include "serd/node.h"
#include "zix/attributes.h"

#include <stddef.h>

/// Return a string length padded to the number used with termination in a node
ZIX_CONST_FUNC size_t
serd_node_pad_length(size_t n_bytes);

/// Return a mutable pointer to the string buffer of a node
ZIX_CONST_FUNC char* ZIX_NONNULL
serd_node_buffer(SerdNode* ZIX_NONNULL node);

/// Return a pointer to the "meta" node for a node (datatype or language)
ZIX_PURE_FUNC const SerdNode* ZIX_UNSPECIFIED
serd_node_meta(const SerdNode* ZIX_NONNULL node);

/// Set the header of a node, completely overwriting previous values
void
serd_node_set_header(SerdNode* ZIX_NONNULL node,
                     size_t                length,
                     SerdNodeFlags         flags,
                     SerdNodeType          type);

/// Set `dst` to be equal to `src`, re-allocating it if necessary
void
serd_node_set(SerdNode* ZIX_NONNULL* ZIX_NONNULL dst,
              const SerdNode* ZIX_NONNULL        src);

/// Retrieve the value of a node as a particular binary datatype if possible
ExessResult
serd_node_get_value_as(const SerdNode* ZIX_NONNULL node,
                       ExessDatatype               value_type,
                       size_t                      value_size,
                       void* ZIX_NONNULL           value);

#endif // SERD_SRC_NODE_H
