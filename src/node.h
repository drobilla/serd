// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_H
#define SERD_SRC_NODE_H

#include "serd/node.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stddef.h>

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

/// Create a new URI from a prefix and suffix (expanded from a CURIE)
SerdNode* ZIX_ALLOCATED
serd_new_expanded_uri(ZixStringView prefix, ZixStringView suffix);

/// Create a new URI from a string, resolved against a base URI
SerdNode* ZIX_ALLOCATED
serd_new_resolved_uri(ZixStringView string, SerdURIView base_uri);

#endif // SERD_SRC_NODE_H
