// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_H
#define SERD_SRC_NODE_H

#include "exess/exess.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/uri.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct SerdNodeImpl {
  size_t        length; ///< Length in bytes (not including null)
  SerdNodeFlags flags;  ///< Node flags
  SerdNodeType  type;   ///< Node type
};

static const size_t serd_node_align = 2 * sizeof(uint64_t);

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

static inline const char* ZIX_NONNULL
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

SerdNode* ZIX_ALLOCATED
serd_node_malloc(ZixAllocator* ZIX_NULLABLE allocator,
                 size_t                     length,
                 SerdNodeFlags              flags,
                 SerdNodeType               type);

SerdStatus
serd_node_set(ZixAllocator* ZIX_NULLABLE         allocator,
              SerdNode* ZIX_NONNULL* ZIX_NONNULL dst,
              const SerdNode* ZIX_NONNULL        src);

void
serd_node_zero_pad(SerdNode* ZIX_NONNULL node);

/// Create a new URI from a string, resolved against a base URI
SerdNode* ZIX_ALLOCATED
serd_new_resolved_uri(ZixAllocator* ZIX_NULLABLE allocator,
                      ZixStringView              string,
                      SerdURIView                base_uri);

ExessResult
serd_node_get_value_as(const SerdNode* ZIX_NONNULL node,
                       ExessDatatype               value_type,
                       size_t                      value_size,
                       void* ZIX_NONNULL           value);

#endif // SERD_SRC_NODE_H
