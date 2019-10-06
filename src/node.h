// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_H
#define SERD_SRC_NODE_H

#include "exess/exess.h"
#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/string_view.h"
#include "serd/uri.h"

#include <stddef.h>
#include <stdint.h>

struct SerdNodeImpl {
  size_t        length; ///< Length in bytes (not including null)
  SerdNodeFlags flags;  ///< Node flags
  SerdNodeType  type;   ///< Node type
};

static const size_t serd_node_align = 2 * sizeof(uint64_t);

static inline char* SERD_NONNULL
serd_node_buffer(SerdNode* SERD_NONNULL node)
{
  return (char*)(node + 1);
}

static inline const char* SERD_NONNULL
serd_node_buffer_c(const SerdNode* SERD_NONNULL node)
{
  return (const char*)(node + 1);
}

SerdNode* SERD_ALLOCATED
serd_node_malloc(size_t length, SerdNodeFlags flags, SerdNodeType type);

void
serd_node_set(SerdNode* SERD_NULLABLE* SERD_NONNULL dst,
              const SerdNode* SERD_NULLABLE         src);

void
serd_node_zero_pad(SerdNode* SERD_NONNULL node);

/// Create a new URI from a string, resolved against a base URI
SerdNode* SERD_ALLOCATED
serd_new_resolved_uri(SerdStringView string, SerdURIView base_uri);

ExessResult
serd_node_get_value_as(const SerdNode* SERD_NONNULL node,
                       ExessDatatype                value_type,
                       size_t                       value_size,
                       void* SERD_NONNULL           value);

#endif // SERD_SRC_NODE_H
