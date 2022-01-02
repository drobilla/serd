// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include "serd/attributes.h"
#include "serd/uri.h"
#include "serd/value.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_node Node
   @ingroup serd
   @{
*/

/**
   An RDF node.

   A node is a single contiguous chunk of data, but the representation is
   opaque to the public.
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

     A literal optionally has either a language, or a datatype (not both).
  */
  SERD_LITERAL = 1,

  /**
     URI (absolute or relative).

     Value is an unquoted URI string, which is either a relative reference
     with respect to the current base URI (e.g. "foo/bar"), or an absolute
     URI (e.g. "http://example.org/foo").
     @see [RFC3986](http://tools.ietf.org/html/rfc3986)
  */
  SERD_URI = 2,

  /**
     A blank node.

     Value is a blank node ID without any syntactic prefix, like "id3", which
     is meaningful only within this serialisation.  @see [RDF 1.1
     Turtle](http://www.w3.org/TR/turtle/#grammar-production-BLANK_NODE_LABEL)
  */
  SERD_BLANK = 3,

  /**
     A variable node.

     Value is a variable name without any syntactic prefix, like "name",
     which is meaningful only within this serialisation.  @see [SPARQL 1.1
     Query Language](https://www.w3.org/TR/sparql11-query/#rVar)
  */
  SERD_VARIABLE = 4,
} SerdNodeType;

/// Node flags, which ORed together make a #SerdNodeFlags
typedef enum {
  SERD_IS_LONG      = 1U << 0U, ///< Literal node should be triple-quoted
  SERD_HAS_DATATYPE = 1U << 1U, ///< Literal node has datatype
  SERD_HAS_LANGUAGE = 1U << 2U, ///< Literal node has language
} SerdNodeFlag;

/// Bitwise OR of SerdNodeFlag values
typedef uint32_t SerdNodeFlags;

/**
   A status code with an associated byte count.

   This is returned by functions which write to a buffer to inform the caller
   about the size written, or in case of overflow, size required.
*/
typedef struct {
  /**
     Status code.

     This reports the status of the operation as usual, and also dictates the
     meaning of `count`.
  */
  SerdStatus status;

  /**
     Number of bytes written or required.

     On success, this is the total number of bytes written.  On #SERD_OVERFLOW,
     this is the number of bytes of output space that are required for success.
  */
  size_t count;
} SerdWriteResult;

/**
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

   This is the universal node constructor which can construct any node.  An
   error will be returned if the parameters do not make sense.  In particular,
   #SERD_HAS_DATATYPE or #SERD_HAS_LANGUAGE (but not both) may only be given if
   `type` is #SERD_LITERAL, and `meta` must be syntactically valid based on
   that flag.

   This function may also be used to determine the size of buffer required by
   passing a null buffer with zero size.

   @param buf_size The size of `buf` in bytes, or zero to only measure.

   @param buf Buffer where the node will be written, or null to only measure.

   @param type The type of the node to construct.

   @param string The string body of the node.

   @param flags Flags that describe the details of the node.

   @param meta The string value of the literal's metadata.  If
   #SERD_HAS_DATATYPE is set, then this must be an absolute datatype URI.  If
   #SERD_HAS_LANGUAGE is set, then this must be a language tag like "en-ca".
   Otherwise, it is ignored.

   @return A result with a `status` and a `count` of bytes written.  If the
   buffer is too small for the node, then `status` will be #SERD_OVERFLOW, and
   `count` will be set to the number of bytes required to successfully
   construct the node.
*/
SERD_API
SerdWriteResult
serd_node_construct(size_t              buf_size,
                    void* SERD_NULLABLE buf,
                    SerdNodeType        type,
                    SerdStringView      string,
                    SerdNodeFlags       flags,
                    SerdStringView      meta);

/**
   Construct a simple "token" node.

   "Token" is just a shorthand used in this API to refer to a node that is not
   a typed or tagged literal, that is, a node that is just one string.  This
   can be used to create URIs, blank nodes, variables, and simple string
   literals.

   Note that string literals constructed with this function will have no flags
   set, and so will be written as "short" literals (not triple-quoted).  To
   construct long literals, use the more advanced serd_construct_literal() with
   the #SERD_IS_LONG flag.

   See the serd_node_construct() documentation for details on buffer usage and
   the return value.
*/
SERD_API
SerdWriteResult
serd_node_construct_token(size_t              buf_size,
                          void* SERD_NULLABLE buf,
                          SerdNodeType        type,
                          SerdStringView      string);

/**
   Construct a URI node from a parsed URI.

   This is similar to serd_node_construct_token(), but will write a parsed URI
   into the new node.  This can be used to resolve a relative URI reference or
   expand a CURIE directly into a node without needing to allocate the URI
   string separately.
*/
SerdWriteResult
serd_node_construct_uri(size_t              buf_size,
                        void* SERD_NULLABLE buf,
                        SerdURIView         uri);

/**
   Construct a file URI node from a path and optional hostname.

   This is similar to serd_node_construct_token(), but will create a new file
   URI from a file path and optional hostname, performing any necessary
   escaping.
*/
SerdWriteResult
serd_node_construct_file_uri(size_t              buf_size,
                             void* SERD_NULLABLE buf,
                             SerdStringView      path,
                             SerdStringView      hostname);

/**
   Construct a literal node with an optional datatype or language.

   Either a datatype (which must be an absolute URI) or a language (which must
   be an RFC5646 language tag) may be given, but not both.

   This is the most general literal constructor, which can be used to construct
   any literal node.  This works like serd_node_construct(), see its
   documentation for details.
*/
SERD_API
SerdWriteResult
serd_node_construct_literal(size_t              buf_size,
                            void* SERD_NULLABLE buf,
                            SerdStringView      string,
                            SerdNodeFlags       flags,
                            SerdStringView      meta);

/**
   Construct a canonical literal for a primitive value.

   The constructed node will be a typed literal in canonical form for the xsd
   datatype corresponding to the value.
*/
SerdWriteResult
serd_node_construct_value(size_t              buf_size,
                          void* SERD_NULLABLE buf,
                          SerdValue           value);

/**
   Construct a canonical xsd:decimal literal.

   The constructed node will be an xsd:decimal literal, like "12.34", with
   datatype xsd:decimal.

   The node will always contain a '.', start with a digit, and end with a digit
   (a leading and/or trailing '0' will be added if necessary), for example,
   "1.0".  It will never be in scientific notation.

   This is a convenience wrapper for serd_node_construct_literal() that
   constructs a node directly from a `double`.
*/
SerdWriteResult
serd_node_construct_decimal(size_t              buf_size,
                            void* SERD_NULLABLE buf,
                            double              value);

/**
   Construct a canonical xsd:integer literal.

   The constructed node will be an xsd:integer literal like "1234", with the
   given datatype, or datatype xsd:integer if none is given.  It is the
   caller's responsibility to ensure that the value is within the range of the
   given datatype.
*/
SerdWriteResult
serd_node_construct_integer(size_t              buf_size,
                            void* SERD_NULLABLE buf,
                            int64_t             value);
/**
   Construct a canonical xsd:hexBinary literal.

   The constructed node will be an xsd:hexBinary literal like "534D", with
   datatype xsd:hexBinary.
*/
SerdWriteResult
serd_node_construct_hex(size_t                   buf_size,
                        void* SERD_NULLABLE      buf,
                        size_t                   value_size,
                        const void* SERD_NONNULL value);

/**
   Construct a canonical xsd:base64Binary literal.

   The constructed node will be an xsd:base64Binary literal like "Zm9vYmFy",
   with datatype xsd:base64Binary.
*/
SerdWriteResult
serd_node_construct_base64(size_t                   buf_size,
                           void* SERD_NULLABLE      buf,
                           size_t                   value_size,
                           const void* SERD_NONNULL value);

/**
   @}
   @defgroup serd_node_allocation Dynamic Allocation

   This is a convenient higher-level node construction API which allocates
   nodes with an allocator.  The returned nodes must be freed with
   serd_node_free() using the same allocator.

   Note that in most cases it is better to use a #SerdNodes instead of managing
   individual node allocations.

   @{
*/

/**
   Create a new node of any type.

   This is a wrapper for serd_node_construct() that allocates a new node on the
   heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_new(SerdAllocator* SERD_NULLABLE allocator,
              SerdNodeType                 type,
              SerdStringView               string,
              SerdNodeFlags                flags,
              SerdStringView               meta);

/**
   Create a new simple "token" node.

   This is a wrapper for serd_node_construct_token() that allocates a new node
   on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_token(SerdAllocator* SERD_NULLABLE allocator,
               SerdNodeType                 type,
               SerdStringView               string);

/**
   Create a new string literal node.

   This is a trivial wrapper for serd_new_token() that passes `SERD_LITERAL`
   for the type.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_string(SerdAllocator* SERD_NULLABLE allocator, SerdStringView string);

/**
   Create a new URI node from a string.

   This is a wrapper for serd_node_construct_uri() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_uri(SerdAllocator* SERD_NULLABLE allocator, SerdStringView string);

/**
   Create a new URI node from a parsed URI.

   This is a wrapper for serd_node_construct_uri() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_parsed_uri(SerdAllocator* SERD_NULLABLE allocator, SerdURIView uri);

/**
   Create a new file URI node from a path and optional hostname.

   This is a wrapper for serd_node_construct_file_uri() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_file_uri(SerdAllocator* SERD_NULLABLE allocator,
                  SerdStringView               path,
                  SerdStringView               hostname);

/**
   Create a new literal node.

   This is a wrapper for serd_node_construct_literal() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_literal(SerdAllocator* SERD_NULLABLE allocator,
                 SerdStringView               string,
                 SerdNodeFlags                flags,
                 SerdStringView               meta);

/**
   Create a new canonical value node.

   This is a wrapper for serd_node_construct_value() that allocates a new node
   on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_value(SerdAllocator* SERD_NULLABLE allocator, SerdValue value);

/**
   Create a new canonical xsd:decimal literal.

   This is a wrapper for serd_node_construct_decimal() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_decimal(SerdAllocator* SERD_NULLABLE allocator, double d);

/**
   Create a new canonical xsd:integer literal.

   This is a wrapper for serd_node_construct_integer() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_integer(SerdAllocator* SERD_NULLABLE allocator, int64_t i);

/**
   Create a new canonical xsd:hexBinary literal.

   This is a wrapper for serd_node_construct_hex() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_hex(SerdAllocator* SERD_NULLABLE allocator,
             const void* SERD_NONNULL     buf,
             size_t                       size);

/**
   Create a new canonical xsd:base64Binary literal.

   This is a wrapper for serd_node_construct_base64() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_base64(SerdAllocator* SERD_NULLABLE allocator,
                const void* SERD_NONNULL     buf,
                size_t                       size);

/**
   @}
*/

/**
   Return the primitive value of a literal node.

   This will return a typed numeric value if the node can be read as one, or
   nothing otherwise.

   @return The primitive value of `node`, if possible and supported.
*/
SERD_API
SerdValue
serd_node_value(const SerdNode* SERD_NONNULL node);

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
SERD_API
SerdValue
serd_node_value_as(const SerdNode* SERD_NONNULL node,
                   SerdValueType                type,
                   bool                         lossy);

/**
   Return the maximum size of a decoded hex or base64 binary node in bytes.

   This returns an upper bound on the number of bytes that would be decoded by
   serd_node_decode().  This is calculated as a simple constant-time arithmetic
   expression based on the length of the encoded string, so may be larger than
   the actual size of the data due to things like additional whitespace.

   @return The size of the decoded hex or base64 blob `node`, or zero if it
   does not have datatype <http://www.w3.org/2001/XMLSchema#hexBinary> or
   <http://www.w3.org/2001/XMLSchema#base64Binary>.
*/
SERD_PURE_API
size_t
serd_node_decoded_size(const SerdNode* SERD_NONNULL node);

/**
   Decode a binary (base64 or hex) node.

   This function can be used to decode a node created with serd_new_base64() or
   serd_new_hex() and retrieve the original unencoded binary data.

   @param node A literal node which is an encoded base64 or hex string.

   @param buf_size The size of `buf` in bytes.

   @param buf Buffer where decoded data will be written.

   @return On success, #SERD_SUCCESS is returned along with the number of bytes
   written.  If the output buffer is too small, then #SERD_OVERFLOW is returned
   along with the number of bytes required for successful decoding.
*/
SERD_API
SerdWriteResult
serd_node_decode(const SerdNode* SERD_NONNULL node,
                 size_t                       buf_size,
                 void* SERD_NONNULL           buf);

/// Return a deep copy of `node`
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_copy(SerdAllocator* SERD_NULLABLE  allocator,
               const SerdNode* SERD_NULLABLE node);

/// Free any data owned by `node`
SERD_API
void
serd_node_free(SerdAllocator* SERD_NULLABLE allocator,
               SerdNode* SERD_NULLABLE      node);

/// Return the type of a node
SERD_PURE_API
SerdNodeType
serd_node_type(const SerdNode* SERD_NONNULL node);

/// Return the string contents of a node
SERD_CONST_API
const char* SERD_NONNULL
serd_node_string(const SerdNode* SERD_NONNULL node);

/// Return the length of the node's string in bytes (excluding terminator)
SERD_PURE_API
size_t
serd_node_length(const SerdNode* SERD_NULLABLE node);

/**
   Return a view of the string in a node.

   This is a convenience wrapper for serd_node_string() and serd_node_length()
   that can be used to get both in a single call.
*/
SERD_PURE_API
SerdStringView
serd_node_string_view(const SerdNode* SERD_NONNULL node);

/**
   Return a parsed view of the URI in a node.

   It is best to check the node type before calling this function, though it is
   safe to call on non-URI nodes.  In that case, it will return a null view
   with all fields zero.

   Note that this parses the URI string contained in the node, so it is a good
   idea to keep the value if you will be using it several times in the same
   scope.
*/
SERD_PURE_API
SerdURIView
serd_node_uri_view(const SerdNode* SERD_NONNULL node);

/// Return the additional flags of a node
SERD_PURE_API
SerdNodeFlags
serd_node_flags(const SerdNode* SERD_NONNULL node);

/**
   Return the optional datatype of a literal node.

   The datatype, if present, is always a URI, typically something like
   <http://www.w3.org/2001/XMLSchema#boolean>.
*/
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_datatype(const SerdNode* SERD_NONNULL node);

/**
   Return the optional language tag of a literal node.

   The language tag, if present, is a well-formed BCP 47 (RFC 4647) language
   tag like "en-ca".  Note that these must be handled case-insensitively, for
   example, the common form "en-CA" is valid, but lowercase is considered
   canonical here.
*/
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_language(const SerdNode* SERD_NONNULL node);

/**
   Return true iff `a` is equal to `b`.

   For convenience, either argument may be null, which isn't considered equal
   to any node.

   @return True if `a` and `b` point to equal nodes, or are both null.
*/
SERD_PURE_API
bool
serd_node_equals(const SerdNode* SERD_NULLABLE a,
                 const SerdNode* SERD_NULLABLE b);

/**
   Compare two nodes.

   Returns less than, equal to, or greater than zero if `a` is less than, equal
   to, or greater than `b`, respectively.

   Nodes are ordered first by type, then by string, then by language or
   datatype, if present.
*/
SERD_PURE_API
int
serd_node_compare(const SerdNode* SERD_NONNULL a,
                  const SerdNode* SERD_NONNULL b);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_H
