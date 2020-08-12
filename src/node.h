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

#include <stddef.h>

struct SerdNodeImpl {
  size_t        length; ///< Length in bytes (not including null)
  SerdNodeFlags flags;  ///< Node flags
  SerdNodeType  type;   ///< Node type
};

static const size_t serd_node_align = 2 * sizeof(size_t);

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
serd_node_set(SerdNode* SERD_NONNULL* SERD_NONNULL dst,
              const SerdNode* SERD_NULLABLE        src);

SERD_PURE_FUNC
size_t
serd_node_total_size(const SerdNode* SERD_NULLABLE node);

void
serd_node_zero_pad(SerdNode* SERD_NONNULL node);

/// Create a new URI from a string, resolved against a base URI
SerdNode* SERD_ALLOCATED
serd_new_resolved_uri(SerdStringView string, SerdURIView base_uri);

SerdNode* SERD_ALLOCATED
serd_new_typed_literal_expanded(SerdStringView str,
                                SerdNodeFlags  flags,
                                SerdNodeType   datatype_type,
                                SerdStringView datatype_prefix,
                                SerdStringView datatype_suffix);

SerdNode* SERD_ALLOCATED
serd_new_typed_literal_uri(SerdStringView str,
                           SerdNodeFlags  flags,
                           SerdURIView    datatype_uri);

ExessVariant
serd_node_get_value_as(const SerdNode* SERD_NONNULL node,
                       ExessDatatype                value_type);

#endif // SERD_NODE_H
