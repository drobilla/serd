// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_H
#define SERD_SRC_NODE_H

#include "serd/node.h"
#include "zix/attributes.h"

#include <stddef.h>

struct SerdNodeImpl {
  size_t        n_bytes; /**< Size in bytes (not including null) */
  SerdNodeFlags flags;   /**< Node flags (e.g. string properties) */
  SerdNodeType  type;    /**< Node type */
};

static inline char* ZIX_NONNULL
serd_node_buffer(SerdNode* ZIX_NONNULL node)
{
  return (char*)(node + 1);
}

static inline const char* ZIX_NONNULL
serd_node_buffer_c(const SerdNode* ZIX_NONNULL node)
{
  return (const char*)(node + 1);
}

SerdNode* ZIX_ALLOCATED
serd_node_malloc(size_t n_bytes, SerdNodeFlags flags, SerdNodeType type);

void
serd_node_set(SerdNode* ZIX_NULLABLE* ZIX_NONNULL dst,
              const SerdNode* ZIX_NULLABLE        src);

#endif // SERD_SRC_NODE_H
