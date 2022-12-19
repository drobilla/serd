// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include "serd/attributes.h"
#include "serd/uri.h"
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
   Type of a node.

   An abstract RDF node can be either a resource or a literal.  This type is
   more precise to preserve syntactic differences and support additional
   features.
*/
typedef enum {
  /**
     Literal value.

     A literal is a string that optionally has either a language, or a datatype
     (but never both).  Literals can only occur as the object of a statement,
     never the subject or predicate.
  */
  SERD_LITERAL = 1U,

  /**
     Universal Resource Identifier (URI).

     A URI (more pedantically, a URI reference) is either a relative reference
     with respect to some base URI, like "foo/bar", or an absolute URI with a
     scheme, like "http://example.org/foo".

     @see [RFC3986](http://tools.ietf.org/html/rfc3986)
  */
  SERD_URI,

  /**
     CURIE, a shortened URI.

     Value is an unquoted CURIE string relative to the current environment,
     e.g. "rdf:type".  @see [CURIE Syntax 1.0](http://www.w3.org/TR/curie)
  */
  SERD_CURIE,

  /**
     A blank node.

     A blank node is a resource that has no URI.  The identifier of a blank
     node is local to its context (a document, for example), and so unlike
     URIs, blank nodes can't be used to link data across sources.

     @see [RDF 1.1
     Turtle](http://www.w3.org/TR/turtle/#grammar-production-BLANK_NODE_LABEL)
  */
  SERD_BLANK,
} SerdNodeType;

/// Node flags, which ORed together make a #SerdNodeFlags
typedef enum {
  SERD_HAS_NEWLINE  = 1U << 0U, ///< Contains line breaks ('\\n' or '\\r')
  SERD_HAS_QUOTE    = 1U << 1U, ///< Contains quotes ('"')
  SERD_HAS_DATATYPE = 1U << 2U, ///< Literal node has datatype
  SERD_HAS_LANGUAGE = 1U << 3U, ///< Literal node has language
} SerdNodeFlag;

/// Bitwise OR of #SerdNodeFlag values
typedef uint32_t SerdNodeFlags;

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
serd_new_token(SerdNodeType type, ZixStringView string);

/**
   Create a new string literal node.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_string(ZixStringView string);

/**
   Create a new plain literal node from `str` with `lang`.

   A plain literal has no datatype, but may have a language tag.  The `lang`
   may be null, in which case this is equivalent to `serd_new_string()`.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_plain_literal(ZixStringView str, const SerdNode* ZIX_NULLABLE lang);

/**
   Create a new typed literal node from `str`.

   A typed literal has no language tag, but may have a datatype.  The
   `datatype` may be NULL, in which case this is equivalent to
   `serd_new_string()`.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_typed_literal(ZixStringView                str,
                       const SerdNode* ZIX_NULLABLE datatype_uri);

/**
   Create a new node from a blank node label.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_blank(ZixStringView string);

/// Create a new CURIE node
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_curie(ZixStringView string);

/**
   Create a new URI node from a parsed URI.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_parsed_uri(SerdURIView uri);

/**
   Create a new URI node from a string.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_uri(ZixStringView string);

/**
   Create a new file URI node from a file system path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_file_uri(ZixStringView path, ZixStringView hostname);

/**
   Create a new canonical xsd:boolean node.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_boolean(bool b);

/**
   Create a new canonical xsd:decimal literal.

   The node will be an xsd:decimal literal, like "12.34", with
   datatype xsd:decimal by default, or a custom datatype.

   The node will always contain a '.', start with a digit, and end with a digit
   (a leading and/or trailing '0' will be added if necessary), for example,
   "1.0".  It will never be in scientific notation.

   @param d The value for the new node.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_decimal(double d);

/**
   Create a new canonical xsd:double literal.

   The node will be in scientific notation, like "1.23E4", except for NaN and
   negative/positive infinity, which are "NaN", "-INF", and "INF",
   respectively.

   Uses the shortest possible representation that precisely describes the
   value, which has at most 17 significant digits (under 24 characters total).

   @param d Double value to write.
   @return A literal node with datatype xsd:double.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_double(double d);

/**
   Create a new canonical xsd:float literal.

   Uses identical formatting to serd_new_double(), except with at most 9
   significant digits (under 14 characters total).

   @param f Float value of literal.
   @return A literal node with datatype xsd:float.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_float(float f);

/**
   Create a new canonical xsd:integer literal.

   The node will be an xsd:integer literal like "1234", with datatype
   xsd:integer.

   @param i Integer value of literal.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_integer(int64_t i);

/**
   Create a new canonical xsd:base64Binary literal.

   This function can be used to make a node out of arbitrary binary data, which
   can be decoded using serd_base64_decode().

   @param buf Raw binary data to encode in node.
   @param size Size of `buf` in bytes.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_new_base64(const void* ZIX_NONNULL buf, size_t size);

/// Return a deep copy of `node`
SERD_API SerdNode* ZIX_ALLOCATED
serd_node_copy(const SerdNode* ZIX_NULLABLE node);

/**
   Free any data owned by `node`.
*/
SERD_API void
serd_node_free(SerdNode* ZIX_NULLABLE node);

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
   Return the value of `node` as a boolean.

   This will work for booleans, and numbers of any datatype if they are 0 or
   1.

   @return The value of `node` as a `bool`, or `false` on error.
*/
SERD_API bool
serd_get_boolean(const SerdNode* ZIX_NONNULL node);

/**
   Return the value of `node` as a double.

   This will coerce numbers of any datatype to double, if the value fits.

   @return The value of `node` as a `double`, or NaN on error.
*/
SERD_API double
serd_get_double(const SerdNode* ZIX_NONNULL node);

/**
   Return the value of `node` as a float.

   This will coerce numbers of any datatype to float, if the value fits.

   @return The value of `node` as a `float`, or NaN on error.
*/
SERD_API float
serd_get_float(const SerdNode* ZIX_NONNULL node);

/**
   Return the value of `node` as a long (signed 64-bit integer).

   This will coerce numbers of any datatype to long, if the value fits.

   @return The value of `node` as a `int64_t`, or 0 on error.
*/
SERD_API int64_t
serd_get_integer(const SerdNode* ZIX_NONNULL node);

/**
   @}
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_H
