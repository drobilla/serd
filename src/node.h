// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

// IWYU pragma: no_include "node_impl.h"

#include <serd/node_args.h>
#include <serd/stream_result.h>
#include <serd/token_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stddef.h>

/// An RDF stored with its string in a single contiguous chunk of memory
typedef struct SerdNodeImpl SerdNode;

// Construction

/**
   Construct a node into an existing buffer.

   This is the universal node constructor which can be used to construct any
   node into an existing buffer.  The required buffer size can be determined by
   passing a null buffer with zero size.

   @param args Arguments describing the node to construct.
   @param buf_size The size of `buf` in bytes, or zero to measure.
   @param[out] buf Buffer where the node will be written, or null to measure.

   @return A result with a `status` and a `count` of bytes written.  If the
   buffer is too small, then `status` will be #SERD_NO_SPACE, and `count` will
   be set to the number of bytes required to successfully construct the node.
   If a value can't be constructed at all, then the `count` will be zero.
*/
SerdStreamResult
serd_node_construct(SerdNodeArgs args, size_t buf_size, void* ZIX_NULLABLE buf);

// Access

/// Return the string contents of a node
ZIX_CONST_FUNC const char* ZIX_NONNULL
serd_node_string(const SerdNode* ZIX_NONNULL node);

/// Return a view of the string in a node
ZIX_PURE_FUNC ZixStringView
serd_node_string_view(const SerdNode* ZIX_NONNULL node);

/**
   Return a view of the node as a token.

   A "token" is a simple node with just a type and one string.  Any node can be
   viewed as a token, but viewing an object as a token will drop any datatype
   URI, language tag, or flags.

   The subject, predicate, and graph of a statement are tokens.
*/
ZIX_PURE_FUNC SerdTokenView
serd_node_token_view(const SerdNode* ZIX_NONNULL node);

/**
   @}
   @}
*/

#endif // SERD_NODE_H
