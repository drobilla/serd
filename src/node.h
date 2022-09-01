// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include "serd/serd.h"

#include <stddef.h>

struct SerdNodeImpl {
  size_t        n_bytes; /**< Size in bytes (not including null) */
  SerdNodeFlags flags;   /**< Node flags (e.g. string properties) */
  SerdType      type;    /**< Node type */
};

static inline char*
serd_node_buffer(SerdNode* node)
{
  return (char*)(node + 1);
}

static inline const char*
serd_node_buffer_c(const SerdNode* node)
{
  return (const char*)(node + 1);
}

SerdNode*
serd_node_malloc(size_t n_bytes, SerdNodeFlags flags, SerdType type);

void
serd_node_set(SerdNode** dst, const SerdNode* src);

#endif // SERD_NODE_H
