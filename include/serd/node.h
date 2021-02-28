// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include "serd/attributes.h"
#include "serd/uri.h"

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
   Type of a node.

   An RDF node, in the abstract sense, can be either a resource, literal, or a
   blank.  This type is more precise, because syntactically there are two ways
   to refer to a resource (by URI or CURIE).

   There are also two ways to refer to a blank node in syntax (by ID or
   anonymously), but this is handled by statement flags rather than distinct
   node types.
*/
typedef enum {
  /**
     The type of a nonexistent node.

     This type is useful as a sentinel, but is never emitted by the reader.
  */
  SERD_NOTHING,

  /**
     Literal value.

     A literal is a string that optionally has either a language, or a datatype
     (but never both).  Literals can only occur as the object of a statement,
     never the subject or predicate.
  */
  SERD_LITERAL,

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
  SERD_HAS_NEWLINE = 1U << 0U, ///< Contains line breaks ('\\n' or '\\r')
  SERD_HAS_QUOTE   = 1U << 1U, ///< Contains quotes ('"')
} SerdNodeFlag;

/// Bitwise OR of #SerdNodeFlag values
typedef uint32_t SerdNodeFlags;

/// A syntactic RDF node
typedef struct {
  const char* SERD_NULLABLE buf;     ///< Value string
  size_t                    n_bytes; ///< Size in bytes (excluding null)
  SerdNodeFlags             flags;   ///< Node flags (string properties)
  SerdNodeType              type;    ///< Node type
} SerdNode;

static const SerdNode SERD_NODE_NULL = {NULL, 0, 0, SERD_NOTHING};

/**
   Make a (shallow) node from `str`.

   This measures, but does not copy, `str`.  No memory is allocated.
*/
SERD_API SerdNode
serd_node_from_string(SerdNodeType type, const char* SERD_NULLABLE str);

/**
   Make a (shallow) node from a prefix of `str`.

   This measures, but does not copy, `str`.  No memory is allocated.
   Note that the returned node may not be null terminated.
*/
SERD_API SerdNode
serd_node_from_substring(SerdNodeType              type,
                         const char* SERD_NULLABLE str,
                         size_t                    len);

/// Simple wrapper for serd_node_new_uri() to resolve a URI node
SERD_API SerdNode
serd_node_new_uri_from_node(const SerdNode* SERD_NONNULL     uri_node,
                            const SerdURIView* SERD_NULLABLE base,
                            SerdURIView* SERD_NULLABLE       out);

/// Simple wrapper for serd_node_new_uri() to resolve a URI string
SERD_API SerdNode
serd_node_new_uri_from_string(const char* SERD_NULLABLE        str,
                              const SerdURIView* SERD_NULLABLE base,
                              SerdURIView* SERD_NULLABLE       out);

/**
   Create a new file URI node from a file system path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.
   If `out` is not NULL, it will be set to the parsed URI.
*/
SERD_API SerdNode
serd_node_new_file_uri(const char* SERD_NONNULL   path,
                       const char* SERD_NULLABLE  hostname,
                       SerdURIView* SERD_NULLABLE out);

/**
   Create a new node by serialising `uri` into a new string.

   @param uri The URI to serialise.

   @param base Base URI to resolve `uri` against (or NULL for no resolution).

   @param out Set to the parsing of the new URI (i.e. points only to
   memory owned by the new returned node).
*/
SERD_API SerdNode
serd_node_new_uri(const SerdURIView* SERD_NONNULL  uri,
                  const SerdURIView* SERD_NULLABLE base,
                  SerdURIView* SERD_NULLABLE       out);

/**
   Create a new node by serialising `d` into an xsd:decimal string.

   The resulting node will always contain a '.', start with a digit, and end
   with a digit (i.e. will have a leading and/or trailing '0' if necessary).
   It will never be in scientific notation.  A maximum of `frac_digits` digits
   will be written after the decimal point, but trailing zeros will
   automatically be omitted (except one if `d` is a round integer).

   Note that about 16 and 8 fractional digits are required to precisely
   represent a double and float, respectively.

   @param d The value for the new node.
   @param frac_digits The maximum number of digits after the decimal place.
*/
SERD_API SerdNode
serd_node_new_decimal(double d, unsigned frac_digits);

/// Create a new node by serialising `i` into an xsd:integer string
SERD_API SerdNode
serd_node_new_integer(int64_t i);

/**
   Create a node by serialising `buf` into an xsd:base64Binary string.

   This function can be used to make a serialisable node out of arbitrary
   binary data, which can be decoded using serd_base64_decode().

   @param buf Raw binary input data.
   @param size Size of `buf`.
   @param wrap_lines Wrap lines at 76 characters to conform to RFC 2045.
*/
SERD_API SerdNode
serd_node_new_blob(const void* SERD_NONNULL buf, size_t size, bool wrap_lines);

/**
   Make a deep copy of `node`.

   @return a node that the caller must free with serd_node_free().
*/
SERD_API SerdNode
serd_node_copy(const SerdNode* SERD_NULLABLE node);

/// Return true iff `a` is equal to `b`
SERD_PURE_API
bool
serd_node_equals(const SerdNode* SERD_NONNULL a,
                 const SerdNode* SERD_NONNULL b);

/**
   Free any data owned by `node`.

   Note that if `node` is itself dynamically allocated (which is not the case
   for nodes created internally by serd), it will not be freed.
*/
SERD_API void
serd_node_free(SerdNode* SERD_NULLABLE node);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_H
