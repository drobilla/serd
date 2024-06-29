// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include "serd/attributes.h"
#include "serd/node_flags.h" // IWYU pragma: export
#include "serd/node_type.h"  // IWYU pragma: export
#include "serd/stream_result.h"
#include "serd/uri.h"
#include "serd/value.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SERD_BEGIN_DECLS

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
   @defgroup serd_node_dynamic_allocation Dynamic Allocation
   @{
*/

/**
   Create a new simple "token" node.

   A "token" is a node that isn't a typed or tagged literal.  This can be used
   to create URIs, blank nodes, CURIEs, and simple string literals.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_token(ZixAllocator* ZIX_NULLABLE allocator,
               SerdNodeType               type,
               ZixStringView              string);

/**
   Create a new string literal node.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_string(ZixAllocator* ZIX_NULLABLE allocator, ZixStringView string);

/**
   Create a new literal node with optional datatype or language.

   This can create more complex literals than serd_new_string() with an
   associated datatype URI or language tag, as well as control whether a
   literal should be written as a short or long (triple-quoted) string.

   @param allocator Allocator for the returned node.

   @param string The string value of the literal.

   @param flags Flags to describe the literal and its metadata.  This must be a
   valid combination of flags, in particular, at most one of #SERD_HAS_DATATYPE
   and #SERD_HAS_LANGUAGE may be set.

   @param meta If #SERD_HAS_DATATYPE is set, then this must be an absolute
   datatype URI.  If #SERD_HAS_LANGUAGE is set, then this must be a language
   tag string like "en-ca".  Otherwise, it is ignored.

   @return A newly allocated literal node that must be freed with
   serd_node_free(), or null if the arguments are invalid or allocation failed.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_literal(ZixAllocator* ZIX_NULLABLE   allocator,
                 ZixStringView                string,
                 SerdNodeFlags                flags,
                 const SerdNode* ZIX_NULLABLE meta);

/**
   Create a new node from a blank node label.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_blank(ZixAllocator* ZIX_NULLABLE allocator, ZixStringView string);

/// Create a new CURIE node
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_curie(ZixAllocator* ZIX_NULLABLE allocator, ZixStringView string);

/**
   Create a new URI node from a parsed URI.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_parsed_uri(ZixAllocator* ZIX_NULLABLE allocator, SerdURIView uri);

/**
   Create a new URI node from a string.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_uri(ZixAllocator* ZIX_NULLABLE allocator, ZixStringView string);

/**
   Create a new file URI node from a file system path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_file_uri(ZixAllocator* ZIX_NULLABLE allocator,
                  ZixStringView              path,
                  ZixStringView              hostname);

/**
   Create a new canonical xsd:boolean node.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_boolean(ZixAllocator* ZIX_NULLABLE allocator, bool b);

/**
   Create a new canonical xsd:decimal literal.

   The node will be an xsd:decimal literal, like "12.34", with
   datatype xsd:decimal by default, or a custom datatype.

   The node will always contain a '.', start with a digit, and end with a digit
   (a leading and/or trailing '0' will be added if necessary), for example,
   "1.0".  It will never be in scientific notation.

   @param allocator Allocator for the returned node.
   @param d The value for the new node.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_decimal(ZixAllocator* ZIX_NULLABLE allocator, double d);

/**
   Create a new canonical xsd:double literal.

   The node will be in scientific notation, like "1.23E4", except for NaN and
   negative/positive infinity, which are "NaN", "-INF", and "INF",
   respectively.

   Uses the shortest possible representation that precisely describes the
   value, which has at most 17 significant digits (under 24 characters total).

   @param allocator Allocator for the returned node.
   @param d Double value to write.
   @return A literal node with datatype xsd:double.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_double(ZixAllocator* ZIX_NULLABLE allocator, double d);

/**
   Create a new canonical xsd:float literal.

   Uses identical formatting to serd_new_double(), except with at most 9
   significant digits (under 14 characters total).

   @param allocator Allocator for the returned node.
   @param f Float value of literal.
   @return A literal node with datatype xsd:float.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_float(ZixAllocator* ZIX_NULLABLE allocator, float f);

/**
   Create a new canonical xsd:integer literal.

   The node will be an xsd:integer literal like "1234", with datatype
   xsd:integer.

   @param allocator Allocator for the returned node.
   @param i Integer value of literal.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_integer(ZixAllocator* ZIX_NULLABLE allocator, int64_t i);

/**
   Create a new canonical xsd:base64Binary literal.

   This function can be used to make a node out of arbitrary binary data, which
   can be decoded using serd_base64_decode().

   @param allocator Allocator for the returned node.
   @param buf Raw binary data to encode in node.
   @param size Size of `buf` in bytes.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_base64(ZixAllocator* ZIX_NULLABLE allocator,
                const void* ZIX_NONNULL    buf,
                size_t                     size);

/**
   Return a deep copy of `node`.

   @param allocator Allocator for the returned node.
   @param node The node to copyl
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_node_copy(ZixAllocator* ZIX_NULLABLE   allocator,
               const SerdNode* ZIX_NULLABLE node);

/**
   Free any data owned by `node`.
*/
SERD_API void
serd_node_free(ZixAllocator* ZIX_NULLABLE allocator,
               SerdNode* ZIX_NULLABLE     node);

/**
   @}
   @defgroup serd_node_operators Operators
   @{
*/

/**
   Return true iff `a` is equal to `b`.

   For convenience, either argument may be null, which isn't considered equal
   to any node.

   @return True if `a` and `b` point to equal nodes, or are both null.
*/
SERD_PURE_API bool
serd_node_equals(const SerdNode* ZIX_NULLABLE a,
                 const SerdNode* ZIX_NULLABLE b);

/**
   Compare two nodes.

   Returns less than, equal to, or greater than zero if `a` is less than, equal
   to, or greater than `b`, respectively.

   Nodes are ordered first by type, then by string, then by language or
   datatype, if present.
*/
SERD_PURE_API int
serd_node_compare(const SerdNode* ZIX_NONNULL a, const SerdNode* ZIX_NONNULL b);

/**
   @}
   @defgroup serd_node_accessors Accessors
   @{
*/

/// Return the type of a node
SERD_PURE_API SerdNodeType
serd_node_type(const SerdNode* ZIX_NONNULL node);

/// Return the additional flags of a node
SERD_PURE_API SerdNodeFlags
serd_node_flags(const SerdNode* ZIX_NONNULL node);

/// Return the length of a node's string in bytes, excluding the terminator
SERD_PURE_API size_t
serd_node_length(const SerdNode* ZIX_NULLABLE node);

/// Return the string contents of a node
SERD_CONST_API const char* ZIX_NONNULL
serd_node_string(const SerdNode* ZIX_NONNULL node);

/**
   Return a view of the string in a node.

   This is a convenience wrapper for serd_node_string() and serd_node_length()
   that can be used to get both in a single call.
*/
SERD_PURE_API ZixStringView
serd_node_string_view(const SerdNode* ZIX_NONNULL node);

/**
   Return a parsed view of the URI in a node.

   It is best to check the node type before calling this function, though it is
   safe to call on non-URI nodes.  In that case, it will return a null view
   with all fields zero.

   Note that this parses the URI string contained in the node, so it is a good
   idea to keep the value if you will be using it several times in the same
   scope.
*/
SERD_PURE_API SerdURIView
serd_node_uri_view(const SerdNode* ZIX_NONNULL node);

/**
   Return the optional datatype of a literal node.

   The datatype, if present, is always a URI, typically something like
   <http://www.w3.org/2001/XMLSchema#boolean>.
*/
SERD_PURE_API const SerdNode* ZIX_NULLABLE
serd_node_datatype(const SerdNode* ZIX_NONNULL node);

/**
   Return the optional language tag of a literal node.

   The language tag, if present, is a well-formed BCP 47 (RFC 4647) language
   tag like "en-ca".  Note that these must be handled case-insensitively, for
   example, the common form "en-CA" is valid, but lowercase is considered
   canonical here.
*/
SERD_PURE_API const SerdNode* ZIX_NULLABLE
serd_node_language(const SerdNode* ZIX_NONNULL node);

/**
   Return the primitive value of a literal node.

   This will return a typed numeric value if the node can be read as one, or
   nothing otherwise.

   @return The primitive value of `node`, if possible and supported.
*/
SERD_API SerdValue
serd_node_value(const SerdNode* ZIX_NONNULL node);

/**
   Return the primitive value of a node as a specific type of number.

   This is like serd_node_value(), but will coerce the value of the node to the
   requested type if possible.

   @param node The node to interpret as a number.

   @param type The desired numeric datatype of the result.

   @param lossy Whether lossy conversions can be used.  If this is false, then
   this function only succeeds if the value could be converted back to the
   original datatype of the node without loss.  Otherwise, precision may be
   reduced or values may be truncated to fit the result.

   @return The value of `node` as a #SerdValue, or nothing.
*/
SERD_API SerdValue
serd_node_value_as(const SerdNode* ZIX_NONNULL node,
                   SerdValueType               type,
                   bool                        lossy);

/**
   Return the maximum size of a decoded binary node in bytes.

   This returns an upper bound on the number of bytes that the node would
   decode to.  This is calculated as a simple constant-time arithmetic
   expression based on the length of the encoded string, so may be larger than
   the actual size of the data due to things like additional whitespace.
*/
SERD_PURE_API size_t
serd_node_decoded_size(const SerdNode* ZIX_NONNULL node);

/**
   Decode a base64 node.

   This function can be used to decode a node created with serd_new_base64().

   @param node A literal node which is an encoded base64 string.

   @param buf_size The size of `buf` in bytes.

   @param buf Buffer where decoded data will be written.

   @return On success, #SERD_SUCCESS is returned along with the number of bytes
   written.  If the output buffer is too small, then #SERD_NO_SPACE is returned
   along with the number of bytes required for successful decoding.
*/
SERD_API SerdStreamResult
serd_node_decode(const SerdNode* ZIX_NONNULL node,
                 size_t                      buf_size,
                 void* ZIX_NONNULL           buf);

/**
   @}
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_H
