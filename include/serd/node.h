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
   @ingroup serd_data
   @{
*/

/**
   @defgroup serd_node_types Types
   @{
*/

/**
   An RDF node.

   A node is represented as a single contiguous chunk of data that contains no
   pointers.  The memory is, however, opaque and of unknown size to the public.
*/
typedef struct SerdNodeImpl SerdNode;

/**
   Type of a node.

   An abstract RDF node can be either a resource, literal, or a blank.  This
   may also be a named variable, which allows patterns to be represented.
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
   @defgroup serd_node_construction_arguments Arguments
   @{
*/

/// The type of a #SerdNodeArgs
typedef enum {
  SERD_NODE_ARGS_TOKEN,
  SERD_NODE_ARGS_PARSED_URI,
  SERD_NODE_ARGS_FILE_URI,
  SERD_NODE_ARGS_LITERAL,
  SERD_NODE_ARGS_PRIMITIVE,
  SERD_NODE_ARGS_DECIMAL,
  SERD_NODE_ARGS_INTEGER,
  SERD_NODE_ARGS_HEX,
  SERD_NODE_ARGS_BASE64,
} SerdNodeArgsType;

typedef struct {
  SerdNodeType   type;
  SerdStringView string;
} SerdNodeTokenArgs;

typedef struct {
  SerdURIView uri;
} SerdNodeParsedURIArgs;

typedef struct {
  SerdStringView path;
  SerdStringView hostname;
} SerdNodeFileURIArgs;

typedef struct {
  SerdStringView string;
  SerdNodeFlags  flags;
  SerdStringView meta;
} SerdNodeLiteralArgs;

typedef struct {
  SerdValue value;
} SerdNodePrimitiveArgs;

typedef struct {
  double value;
} SerdNodeDecimalArgs;

typedef struct {
  int64_t value;
} SerdNodeIntegerArgs;

typedef struct {
  size_t                   size;
  const void* SERD_NONNULL data;
} SerdNodeBlobArgs;

/// The data of a #SerdNodeArgs
typedef union {
  SerdNodeTokenArgs     as_token;
  SerdNodeParsedURIArgs as_parsed_uri;
  SerdNodeFileURIArgs   as_file_uri;
  SerdNodeLiteralArgs   as_literal;
  SerdNodePrimitiveArgs as_primitive;
  SerdNodeDecimalArgs   as_decimal;
  SerdNodeIntegerArgs   as_integer;
  SerdNodeBlobArgs      as_blob;
} SerdNodeArgsData;

/// Arguments for constructing a node
typedef struct {
  SerdNodeArgsType type;
  SerdNodeArgsData data;
} SerdNodeArgs;

/**
   Construct a simple "token" node.

   "Token" is just a shorthand used in this API to refer to a node that is not
   a typed or tagged literal, that is, a node that is just one string.  This
   can be used to create URIs, blank nodes, variables, and simple string
   literals.

   Note that string literals constructed with this function will have no flags
   set, and so will be written as "short" literals (not triple-quoted).  To
   construct long literals, use the more advanced serd_as_literal() with the
   #SERD_IS_LONG flag.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_token(SerdNodeType type, SerdStringView string);

/**
   Construct a URI node from a parsed URI.

   This is similar to serd_as_token(), but will write a parsed URI into the
   new node.  This can be used to resolve a relative URI reference or expand a
   CURIE directly into a node without needing to allocate the URI string
   separately.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_parsed_uri(SerdURIView uri);

/**
   Construct a file URI node from a path and optional hostname.

   This is similar to serd_as_token(), but will create a new file URI from a
   file path and optional hostname, performing any necessary escaping.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_file_uri(SerdStringView path, SerdStringView hostname);

/**
   Construct a literal node with an optional datatype or language.

   Either a datatype (which must be an absolute URI) or a language (which must
   be an RFC5646 language tag) may be given, but not both.

   This is the most general literal constructor, which can be used to construct
   any literal node.

   @param string The string body of the node.

   @param flags Flags that describe the details of the node.

   @param meta The string value of the literal's metadata.  If
   #SERD_HAS_DATATYPE is set, then this must be an absolute datatype URI.  If
   #SERD_HAS_LANGUAGE is set, then this must be a language tag like "en-ca".
   Otherwise, it is ignored.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_literal(SerdStringView string,
                SerdNodeFlags  flags,
                SerdStringView meta);

/**
   Construct a string literal node.

   This is a trivial wrapper for serd_as_token() that passes `SERD_LITERAL`
   for the type.
*/
SERD_CONST_FUNC
static inline SerdNodeArgs
serd_as_string_view(SerdStringView string)
{
  return serd_as_token(SERD_LITERAL, string);
}

/**
   Construct a string literal node.

   This is a trivial wrapper for serd_as_token() that passes `SERD_LITERAL`
   for the type.
*/
SERD_CONST_FUNC
static inline SerdNodeArgs
serd_as_string(const char* SERD_NONNULL string)
{
  return serd_as_string_view(serd_string(string));
}

/**
   Construct a blank node.

   This is a trivial wrapper for serd_as_token() that passes `SERD_BLANK` for
   the type.
*/
SERD_CONST_FUNC
static inline SerdNodeArgs
serd_as_blank(SerdStringView name)
{
  return serd_as_token(SERD_BLANK, name);
}

/**
   Construct a URI node from a string view.

   This is a trivial wrapper for serd_as_token() that passes `SERD_URI` for
   the type.
*/
SERD_CONST_FUNC
static inline SerdNodeArgs
serd_as_uri(SerdStringView uri)
{
  return serd_as_token(SERD_URI, uri);
}

/**
   Construct a URI node from a string.

   This is a trivial wrapper for serd_as_token() that passes `SERD_URI` for
   the type.
*/
SERD_CONST_FUNC
static inline SerdNodeArgs
serd_as_uri_string(const char* SERD_NONNULL uri)
{
  return serd_as_uri(serd_string(uri));
}

/**
   Construct a literal node with a datatype.

   @param string The string body of the node.
   @param datatype The absolute URI of the datatype.
 */
SERD_CONST_FUNC
static inline SerdNodeArgs
serd_as_typed_literal(const SerdStringView string,
                      const SerdStringView datatype)
{
  return serd_as_literal(string, SERD_HAS_DATATYPE, datatype);
}

/**
   Construct a literal node with a language.

   @param string The string body of the node.
   @param language A language tag like "en-ca".
 */
SERD_CONST_FUNC
static inline SerdNodeArgs
serd_as_plain_literal(const SerdStringView string,
                      const SerdStringView language)
{
  return serd_as_literal(string, SERD_HAS_LANGUAGE, language);
}

/**
   Construct a canonical literal for a primitive value.

   The constructed node will be a typed literal in canonical form for the xsd
   datatype corresponding to the value.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_primitive(SerdValue value);

/**
   Construct a canonical xsd:decimal literal.

   The constructed node will be an xsd:decimal literal, like "12.34", with
   datatype xsd:decimal.

   The node will always contain a '.', start with a digit, and end with a digit
   (a leading and/or trailing '0' will be added if necessary), for example,
   "1.0".  It will never be in scientific notation.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_decimal(double value);

/**
   Construct a canonical xsd:integer literal.

   The constructed node will be an xsd:integer literal like "1234", with
   datatype xsd:integer.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_integer(int64_t value);

/**
   Construct a canonical xsd:hexBinary literal.

   The constructed node will be an xsd:hexBinary literal like "534D", with
   datatype xsd:hexBinary.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_hex(size_t size, const void* SERD_NONNULL data);

/**
   Construct a canonical xsd:base64Binary literal.

   The constructed node will be an xsd:base64Binary literal like "Zm9vYmFy",
   with datatype xsd:base64Binary.
*/
SERD_CONST_API
SerdNodeArgs
serd_as_base64(size_t size, const void* SERD_NONNULL data);

/**
   @}
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
   buffer is too small for the node, then `status` will be #SERD_OVERFLOW, and
   `count` will be set to the number of bytes required to successfully
   construct the node.
*/
SERD_API
SerdWriteResult
serd_node_construct(size_t              buf_size,
                    void* SERD_NULLABLE buf,
                    SerdNodeArgs        args);

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
   Create a new node.

   This allocates and constructs a new node of any type.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_new(SerdAllocator* SERD_NULLABLE allocator, SerdNodeArgs args);

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

/**
   @}
   @defgroup serd_node_accessors Accessors
   @{
*/

/// Return the type (URI, literal, blank, or variable) of the node
SERD_PURE_API
SerdNodeType
serd_node_type(const SerdNode* SERD_NONNULL node);

/// Return the length of the node's string in bytes (excluding terminator)
SERD_PURE_API
size_t
serd_node_length(const SerdNode* SERD_NULLABLE node);

/// Return the flags (string properties) of a node
SERD_PURE_API
SerdNodeFlags
serd_node_flags(const SerdNode* SERD_NONNULL node);

/// Return the node's string
SERD_CONST_API
const char* SERD_NONNULL
serd_node_string(const SerdNode* SERD_NONNULL node);

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

/// Return the datatype of the literal node, if present
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_datatype(const SerdNode* SERD_NONNULL node);

/// Return the language tag of the literal node, if present
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_language(const SerdNode* SERD_NONNULL node);

/**
   Return the primitive value of `node`.

   This will return a typed value if the node can be read as one, or a value
   with type #SERD_NOTHING otherwise.

   @return The value of `node` as a #SerdValue, if possible.
*/
SERD_API
SerdValue
serd_node_value(const SerdNode* SERD_NONNULL node);

/**
   Return the value of `node` as a specific type of number.

   This is like serd_get_number(), but will coerce the value of the node to the
   requrested type if possible.

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

   This function can be used to decode a node created with serd_as_base64() or
   serd_as_hex() and retrieve the original unencoded binary data.

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

/**
   @}
   @defgroup serd_node_operators Operators
   @{
*/

/// Return true iff `a` is equal to `b`
SERD_PURE_API
bool
serd_node_equals(const SerdNode* SERD_NULLABLE a,
                 const SerdNode* SERD_NULLABLE b);

/**
   Compare two nodes.

   Returns less than, equal to, or greater than zero if `a` is less than, equal
   to, or greater than `b`, respectively.  NULL is treated as less than any
   other node.

   Nodes are ordered first by type, then by string value, then by language or
   datatype, if present.
*/
SERD_PURE_API
int
serd_node_compare(const SerdNode* SERD_NONNULL a,
                  const SerdNode* SERD_NONNULL b);

/**
   @}
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_H
