// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODES_H
#define SERD_NODES_H

#include "serd/attributes.h"
#include "serd/node.h"

#include <stdbool.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_nodes Nodes
   @ingroup serd
   @{
*/

/// Hashing node container for interning and simplified memory management
typedef struct SerdNodesImpl SerdNodes;

/// Create a new node set
SERD_API
SerdNodes* SERD_ALLOCATED
serd_nodes_new(SerdAllocator* SERD_NULLABLE allocator);

/**
   Free `nodes` and all nodes that are stored in it.

   Note that this invalidates any node pointers previously returned from
   `nodes`.
*/
SERD_API
void
serd_nodes_free(SerdNodes* SERD_NULLABLE nodes);

/// Return the number of interned nodes
SERD_PURE_API
size_t
serd_nodes_size(const SerdNodes* SERD_NONNULL nodes);

/**
   Return the existing interned copy of a node if it exists.

   This either returns an equivalent to the given node, or null if this node
   has not been interned.
*/
SERD_API
const SerdNode* SERD_NULLABLE
serd_nodes_get(const SerdNodes* SERD_NONNULL nodes,
               const SerdNode* SERD_NULLABLE node);

/**
   Intern `node`.

   Multiple calls with equivalent nodes will return the same pointer.

   @return A node that is different than, but equivalent to, `node`.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_intern(SerdNodes* SERD_NONNULL       nodes,
                  const SerdNode* SERD_NULLABLE node);

/**
   Make a simple "token" node.

   "Token" is just a shorthand used in this API to refer to a node that is not
   a typed or tagged literal, that is, a node that is just one string.  This
   can be used to make URIs, blank nodes, variables, and simple string
   literals.

   Note that string literals constructed with this function will have no flags
   set, and so will be written as "short" literals (not triple-quoted).  To
   construct long literals, use the more advanced serd_nodes_literal() with the
   #SERD_IS_LONG flag.

   A new node will be added if an equivalent node is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_token(SerdNodes* SERD_NONNULL nodes,
                 SerdNodeType            type,
                 SerdStringView          string);

/**
   Make a string node.

   A new node will be added if an equivalent node is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_string(SerdNodes* SERD_NONNULL nodes, SerdStringView string);

/**
   Make a URI node from a string.

   A new node will be constructed with serd_node_construct_token() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_uri(SerdNodes* SERD_NONNULL nodes, SerdStringView string);

/**
   Make a URI node from a parsed URI.

   A new node will be constructed with serd_node_construct_uri() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_parsed_uri(SerdNodes* SERD_NONNULL nodes, SerdURIView uri);

/**
   Make a file URI node from a path and optional hostname.

   A new node will be constructed with serd_node_construct_file_uri() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_file_uri(SerdNodes* SERD_NONNULL nodes,
                    SerdStringView          path,
                    SerdStringView          hostname);

/**
   Make a literal node with optional datatype or language.

   This can create complex literals with an associated datatype URI or language
   tag, and control whether a literal should be written as a short or long
   (triple-quoted) string.

   @param nodes The node set to get this literal from.

   @param string The string value of the literal.

   @param flags Flags to describe the literal and its metadata.  Note that at
   most one of #SERD_HAS_DATATYPE and #SERD_HAS_LANGUAGE may be set.

   @param meta The string value of the literal's metadata.  If
   #SERD_HAS_DATATYPE is set, then this must be an absolute datatype URI.  If
   #SERD_HAS_LANGUAGE is set, then this must be an RFC 5646 language tag like
   "en-ca".  Otherwise, it is ignored.

   @return A newly allocated literal node that must be freed with
   serd_node_free(), or null if the arguments are invalid or allocation failed.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_literal(SerdNodes* SERD_NONNULL nodes,
                   SerdStringView          string,
                   SerdNodeFlags           flags,
                   SerdStringView          meta);

/**
   Make a canonical xsd:boolean node.

   A new node will be constructed with serd_node_construct_boolean() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_boolean(SerdNodes* SERD_NONNULL nodes, bool value);

/**
   Make a canonical xsd:decimal node.

   A new node will be constructed with serd_node_construct_decimal() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_decimal(SerdNodes* SERD_NONNULL nodes, double value);

/**
   Make a canonical xsd:double node.

   A new node will be constructed with serd_node_construct_double() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_double(SerdNodes* SERD_NONNULL nodes, double value);

/**
   Make a canonical xsd:float node.

   A new node will be constructed with serd_node_construct_float() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_float(SerdNodes* SERD_NONNULL nodes, float value);

/**
   Make a canonical xsd:integer node.

   A new node will be constructed with serd_node_construct_integer() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_integer(SerdNodes* SERD_NONNULL nodes, int64_t value);

/**
   Make a canonical xsd:base64Binary node.

   A new node will be constructed with serd_node_construct_base64() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_base64(SerdNodes* SERD_NONNULL  nodes,
                  const void* SERD_NONNULL value,
                  size_t                   value_size);

/**
   Make a blank node.

   A new node will be constructed with serd_node_construct_token() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_blank(SerdNodes* SERD_NONNULL nodes, SerdStringView string);

/**
   Dereference `node`.

   Decrements the reference count of `node`, and frees the internally stored
   equivalent node if this was the last reference.  Does nothing if no node
   equivalent to `node` is stored in `nodes`.
*/
SERD_API
void
serd_nodes_deref(SerdNodes* SERD_NONNULL       nodes,
                 const SerdNode* SERD_NULLABLE node);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODES_H
