// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

// IWYU pragma: no_include "node_impl.h"

#include <serd/node_args.h>
#include <serd/stream_result.h>
#include <serd/token_view.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stddef.h>

/**
   @defgroup serd_node Node
   @ingroup serd_data
   @{
*/

/**
   @defgroup serd_node_types Types
   @{
*/

/**
   An RDF node.

   A node in memory is a single contiguous chunk of data, but the
   representation is opaque and may only be accessed through the API.
*/
typedef struct SerdNodeImpl SerdNode;

/**
   @}
   @defgroup serd_node_construction Construction

   This is the low-level node construction API, which can be used to construct
   nodes into existing buffers.  Advanced applications can use this to
   specially manage node memory, for example by allocating nodes on the stack,
   or with a special allocator.

   Note that nodes are "plain old data", so there is no need to destroy a
   constructed node, and nodes may be trivially copied, for example with
   memcpy().

   @{
*/

/**
   Construct a node into an existing buffer.

   This is the universal node constructor which can construct any node.  The
   type of node is specified in a #SerdNodeArgs tagged union, to avoid API
   bloat and allow this function to be used with data-based dispatch.

   This function may also be used to determine the size of buffer required by
   passing a null buffer with zero size.

   @param buf_size The size of `buf` in bytes, or zero to only measure.

   @param buf Buffer where the node will be written, or null to only measure.

   @param args Arguments describing the node to construct.

   @return A result with a `status` and a `count` of bytes written.  If the
   buffer is too small, then `status` will be #SERD_NO_SPACE, and `count` will
   be set to the number of bytes required to successfully construct the node.
   If a value can't be constructed at all, then the `count` will be zero.
*/
SerdStreamResult
serd_node_construct(size_t buf_size, void* ZIX_NULLABLE buf, SerdNodeArgs args);

/**
   @}
   @defgroup serd_node_dynamic_allocation Dynamic Allocation

   This is a convenient higher-level node construction API which allocates
   nodes with an allocator.

   Note that in most cases it is better to use a #SerdNodes instead of managing
   individual node allocations.

   @{
*/

/**
   Create a new node.

   This allocates and constructs a new node of any type.

   @param allocator Allocator for the returned node.
   @param args Description of the node to construct.
   @return A newly allocated node.
*/
SerdNode* ZIX_ALLOCATED
serd_node_new(ZixAllocator* ZIX_NULLABLE allocator, SerdNodeArgs args);

/**
   Return a deep copy of `node`.

   @param allocator Allocator for the returned node.
   @param node The node to copy.
*/
SerdNode* ZIX_ALLOCATED
serd_node_copy(ZixAllocator* ZIX_NULLABLE  allocator,
               const SerdNode* ZIX_NONNULL node);

/**
   @}
   @defgroup serd_node_accessors Accessors
   @{
*/

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
