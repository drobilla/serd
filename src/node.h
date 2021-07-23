/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include "exess/exess.h"
#include "serd/serd.h"

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

static inline size_t
serd_node_pad_length(const size_t n_bytes)
{
  const size_t align = sizeof(SerdNode);

  assert((align & (align - 1u)) == 0u);

  return (n_bytes + align + 2u) & ~(align - 1u);
}

#else

static inline size_t
serd_node_pad_length(const size_t n_bytes)
{
  const size_t pad  = sizeof(SerdNode) - (n_bytes + 2) % sizeof(SerdNode);
  const size_t size = n_bytes + 2 + pad;
  assert(size % sizeof(SerdNode) == 0);
  return size;
}

#endif

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

static inline SerdNode* SERD_NONNULL
serd_node_meta(SerdNode* const SERD_NONNULL node)
{
  return node + 1 + (serd_node_pad_length(node->length) / sizeof(SerdNode));
}

static inline const SerdNode* SERD_NONNULL
serd_node_meta_c(const SerdNode* const SERD_NONNULL node)
{
  assert(node->flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE));
  return node + 1 + (serd_node_pad_length(node->length) / sizeof(SerdNode));
}

static inline const char* SERD_NONNULL
serd_node_string_i(const SerdNode* const SERD_NONNULL node)
{
  return (const char*)(node + 1);
}

static inline bool
serd_node_pattern_match(const SerdNode* SERD_NULLABLE a,
                        const SerdNode* SERD_NULLABLE b)
{
  return !a || !b || serd_node_equals(a, b);
}

SERD_PURE_FUNC
bool
is_langtag(SerdStringView string);

SERD_MALLOC_FUNC
SerdNode* SERD_ALLOCATED
serd_node_malloc(size_t size);

SERD_MALLOC_FUNC
SerdNode* SERD_ALLOCATED
serd_node_try_malloc(SerdWriteResult result);

void
serd_node_set(SerdNode* SERD_NULLABLE* SERD_NONNULL dst,
              const SerdNode* SERD_NULLABLE         src);

SERD_PURE_FUNC
size_t
serd_node_total_size(const SerdNode* SERD_NONNULL node);

void
serd_node_zero_pad(SerdNode* SERD_NONNULL node);

ExessResult
serd_node_get_value_as(const SerdNode* SERD_NONNULL node,
                       ExessDatatype                value_type,
                       size_t                       value_size,
                       void* SERD_NONNULL           value);

#endif // SERD_NODE_H
