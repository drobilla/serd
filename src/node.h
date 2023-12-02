// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_H
#define SERD_SRC_NODE_H

#include "serd/node.h"
#include "serd/status.h"
#include "serd/write_result.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct SerdNodeImpl {
  size_t        length; ///< Length in bytes (not including null)
  SerdNodeFlags flags;  ///< Node flags
  SerdNodeType  type;   ///< Node type
};

static const size_t serd_node_align = 2 * sizeof(uint64_t);

#if SIZE_MAX == UINT64_MAX

/**
   Pad a node string length to the number of bytes it will occupy in a node.

   This returns a size that is at least one larger than `n_bytes` (to ensure
   the string is null terminated), but possibly even larger (to align the node
   size).
*/
static inline size_t
serd_node_pad_length(const size_t n_bytes)
{
  assert((serd_node_align & (serd_node_align - 1U)) == 0U);

  return (n_bytes + serd_node_align) & ~(serd_node_align - 1U);
}

#else

static inline size_t
serd_node_pad_length(const size_t n_bytes)
{
  return (n_bytes + sizeof(SerdNode)) / sizeof(SerdNode) * sizeof(SerdNode);
}

#endif

ZIX_CONST_FUNC static inline char* ZIX_NONNULL
serd_node_buffer(SerdNode* ZIX_NONNULL node)
{
  return (char*)(node + 1);
}

ZIX_PURE_FUNC static inline const char* ZIX_NONNULL
serd_node_buffer_c(const SerdNode* ZIX_NONNULL node)
{
  return (const char*)(node + 1);
}

ZIX_PURE_FUNC static inline SerdNode* ZIX_NONNULL
serd_node_meta(SerdNode* const ZIX_NONNULL node)
{
  return node + 1 + (serd_node_pad_length(node->length) / sizeof(SerdNode));
}

ZIX_PURE_FUNC static inline const SerdNode* ZIX_NONNULL
serd_node_meta_c(const SerdNode* const ZIX_NONNULL node)
{
  assert(node->flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE));
  return node + 1 + (serd_node_pad_length(node->length) / sizeof(SerdNode));
}

ZIX_CONST_FUNC static inline const char* ZIX_NONNULL
serd_node_string_i(const SerdNode* const ZIX_NONNULL node)
{
  return (const char*)(node + 1);
}

static inline bool
serd_node_pattern_match(const SerdNode* ZIX_NULLABLE a,
                        const SerdNode* ZIX_NULLABLE b)
{
  return !a || !b || serd_node_equals(a, b);
}

ZIX_MALLOC_FUNC SerdNode* ZIX_ALLOCATED
serd_node_malloc(ZixAllocator* ZIX_NULLABLE allocator, size_t size);

ZIX_MALLOC_FUNC SerdNode* ZIX_ALLOCATED
serd_node_try_malloc(ZixAllocator* ZIX_NULLABLE allocator,
                     SerdWriteResult            result);

SerdStatus
serd_node_set(ZixAllocator* ZIX_NULLABLE         allocator,
              SerdNode* ZIX_NONNULL* ZIX_NONNULL dst,
              const SerdNode* ZIX_NONNULL        src);

ZIX_PURE_FUNC size_t
serd_node_total_size(const SerdNode* ZIX_NONNULL node);

#endif // SERD_SRC_NODE_H
