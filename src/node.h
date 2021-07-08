// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_H
#define SERD_SRC_NODE_H

#include "serd/node.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stddef.h>
#include <stdint.h>

static const size_t serd_node_align = 2 * sizeof(uint64_t);

ZIX_CONST_FUNC char* ZIX_NONNULL
serd_node_buffer(SerdNode* ZIX_NONNULL node);

ZIX_CONST_FUNC const char* ZIX_NONNULL
serd_node_buffer_c(const SerdNode* ZIX_NONNULL node);

SerdNode* ZIX_ALLOCATED
serd_node_malloc(size_t length, SerdNodeFlags flags, SerdNodeType type);

void
serd_node_set(SerdNode* ZIX_NONNULL* ZIX_NONNULL dst,
              const SerdNode* ZIX_NONNULL        src);

void
serd_node_reset(SerdNode* ZIX_NONNULL node);

/// Create a new URI from a prefix and suffix (expanded from a CURIE)
SerdNode* ZIX_ALLOCATED
serd_new_expanded_uri(ZixStringView prefix, ZixStringView suffix);

/// Create a new URI from a string, resolved against a base URI
SerdNode* ZIX_ALLOCATED
serd_new_resolved_uri(ZixStringView string, SerdURIView base_uri);

#endif // SERD_SRC_NODE_H
