// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include "serd/attributes.h"
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
   Type of a node.

   Note that this set of types is both more precise than, and extended from,
   the possible types of an abstract RDF node.  Not all types can occur in all
   contexts, for example, a Turtle document can't contain a variable node.

   The string value of a node never contains quoting or other type indicators.
   For example, the blank node `_:id3` and the plain literal `"id3"` from a
   Turtle document would both have the same string, "id3", returned by
   #serd_node_string.
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

  /**
     Variable node.

     A variable node, like a blank node, is local to its context.  Variables
     don't occur in data but are used in search patterns.

     @see [SPARQL 1.1 Query
     Language](https://www.w3.org/TR/sparql11-query/#rVar)
  */
  SERD_VARIABLE,
} SerdNodeType;

/// Node flags, which ORed together make a #SerdNodeFlags
typedef enum {
  SERD_IS_LONG      = 1U << 0U, ///< Literal node should be triple-quoted
  SERD_HAS_DATATYPE = 1U << 1U, ///< Literal node has datatype
  SERD_HAS_LANGUAGE = 1U << 2U, ///< Literal node has language
} SerdNodeFlag;

/// Bitwise OR of #SerdNodeFlag values
typedef uint32_t SerdNodeFlags;

/**
   @}
   @defgroup serd_node_construction_arguments Arguments

   A unified representation of the arguments needed to specify any node.

   Since there are several types of node, and several functions that take
   node descriptions as arguments, the arguments to specify a node are
   encapsulated in a single struct to prevent a combinatorial explosion.

   Arguments constructors like #serd_a_file_uri return a temporary view of
   their arguments, which can be passed (usually inline) to node construction
   functions like #serd_node_new, #serd_node_construct, or #serd_nodes_get.

   @{
*/

/// The type of a #SerdNodeArgs
typedef enum {
  SERD_NODE_ARGS_TOKEN,         ///< A token @see #serd_a_token
  SERD_NODE_ARGS_PARSED_URI,    ///< A parsed URI @see #serd_a_parsed_uri
  SERD_NODE_ARGS_FILE_URI,      ///< A file URI @see #serd_a_file_uri
  SERD_NODE_ARGS_PREFIXED_NAME, ///< A prefixed name @see #serd_a_prefixed_name
  SERD_NODE_ARGS_JOINED_URI,    ///< A prefixed name @see #serd_a_joined_uri
  SERD_NODE_ARGS_LITERAL,       ///< A literal @see #serd_a_literal
  SERD_NODE_ARGS_PRIMITIVE,     ///< A "native" primitive @see #serd_a_primitive
  SERD_NODE_ARGS_DECIMAL,       ///< A decimal number @see #serd_a_decimal
  SERD_NODE_ARGS_INTEGER,       ///< An integer number @see #serd_a_integer
  SERD_NODE_ARGS_HEX,           ///< A hex-encoded blob @see #serd_a_hex
  SERD_NODE_ARGS_BASE64,        ///< A base64-encoded blob @see #serd_a_base64
} SerdNodeArgsType;

/// The data for #SERD_NODE_ARGS_TOKEN
typedef struct {
  SerdNodeType  type;
  ZixStringView string;
} SerdNodeTokenArgs;

/// The data for #SERD_NODE_ARGS_PARSED_URI
typedef struct {
  SerdURIView uri;
} SerdNodeParsedURIArgs;

/// The data for #SERD_NODE_ARGS_FILE_URI
typedef struct {
  ZixStringView path;
  ZixStringView hostname;
} SerdNodeFileURIArgs;

/// The data for #SERD_NODE_ARGS_PREFIXED_NAME
typedef struct {
  ZixStringView prefix;
  ZixStringView name;
} SerdNodePrefixedNameArgs;

/// The data for #SERD_NODE_ARGS_JOINED_URI
typedef struct {
  ZixStringView prefix;
  ZixStringView suffix;
} SerdNodeJoinedURIArgs;

/// The data for #SERD_NODE_ARGS_LITERAL
typedef struct {
  ZixStringView                string;
  SerdNodeFlags                flags;
  const SerdNode* ZIX_NULLABLE meta;
} SerdNodeLiteralArgs;

/// The data for #SERD_NODE_ARGS_PRIMITIVE
typedef struct {
  SerdValue value;
} SerdNodePrimitiveArgs;

/// The data for #SERD_NODE_ARGS_DECIMAL
typedef struct {
  double value;
} SerdNodeDecimalArgs;

/// The data for #SERD_NODE_ARGS_INTEGER
typedef struct {
  int64_t value;
} SerdNodeIntegerArgs;

/// The data for #SERD_NODE_ARGS_HEX or #SERD_NODE_ARGS_BASE64
typedef struct {
  size_t                  size;
  const void* ZIX_NONNULL data;
} SerdNodeBlobArgs;

/// The data of a #SerdNodeArgs
typedef union {
  SerdNodeTokenArgs        as_token;
  SerdNodeParsedURIArgs    as_parsed_uri;
  SerdNodeFileURIArgs      as_file_uri;
  SerdNodePrefixedNameArgs as_prefixed_name;
  SerdNodeJoinedURIArgs    as_joined_uri;
  SerdNodeLiteralArgs      as_literal;
  SerdNodePrimitiveArgs    as_primitive;
  SerdNodeDecimalArgs      as_decimal;
  SerdNodeIntegerArgs      as_integer;
  SerdNodeBlobArgs         as_blob;
} SerdNodeArgsData;

/// Arguments for constructing a node
typedef struct {
  SerdNodeArgsType type; ///< Type of node described and valid field of `data`
  SerdNodeArgsData data; ///< Data union
} SerdNodeArgs;

/**
   A simple "token" node.

   "Token" is just a shorthand used in this API to refer to a node that is not
   a typed or tagged literal, that is, a node that is just one string.  This
   can be used to create URIs, blank nodes, variables, and simple string
   literals.

   Note that string literals constructed with this function will have no flags
   set, and so will be written as "short" literals (not triple-quoted).  To
   construct long literals, use the more advanced serd_a_literal() with the
   #SERD_IS_LONG flag.
*/
SERD_CONST_API SerdNodeArgs
serd_a_token(SerdNodeType type, ZixStringView string);

/// A URI node from a parsed URI
SERD_CONST_API SerdNodeArgs
serd_a_parsed_uri(SerdURIView uri);

/// A file URI node from a path and optional hostname
SERD_CONST_API SerdNodeArgs
serd_a_file_uri(ZixStringView path, ZixStringView hostname);

/// A CURIE node from a prefix name and a local name
SERD_CONST_API SerdNodeArgs
serd_a_prefixed_name(ZixStringView prefix, ZixStringView name);

/// A URI from a joined prefix and suffix (an in-place expanded CURIE)
SERD_CONST_API SerdNodeArgs
serd_a_joined_uri(ZixStringView prefix, ZixStringView suffix);

/**
   A literal node with an optional datatype or language.

   Either a datatype (which must be an absolute URI) or a language (which must
   be an RFC5646 language tag) may be given, but not both.

   This is the most general literal constructor, which can be used to construct
   any literal node.

   @param string The string body of the node.

   @param flags Flags that describe the details of the node.

   @param meta If #SERD_HAS_DATATYPE is set, then this must be an absolute
   datatype URI.  If #SERD_HAS_LANGUAGE is set, then this must be a language
   tag like "en-ca".  Otherwise, it is ignored.
*/
SERD_CONST_API SerdNodeArgs
serd_a_literal(ZixStringView                string,
               SerdNodeFlags                flags,
               const SerdNode* ZIX_NULLABLE meta);

/// A simple string literal node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_string_view(ZixStringView string)
{
  return serd_a_token(SERD_LITERAL, string);
}

/// A simple string literal node from a C string
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_string(const char* ZIX_NONNULL string)
{
  return serd_a_string_view(zix_string(string));
}

/// A blank node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_blank(ZixStringView name)
{
  return serd_a_token(SERD_BLANK, name);
}

/// A blank node from a string
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_blank_string(const char* ZIX_NONNULL name)
{
  return serd_a_blank(zix_string(name));
}

/// A URI node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_uri(ZixStringView uri)
{
  return serd_a_token(SERD_URI, uri);
}

/**
   A URI node from a string.

   @param uri The URI string.
*/
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_uri_string(const char* ZIX_NONNULL uri)
{
  return serd_a_uri(zix_string(uri));
}

/// A CURIE node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_curie(ZixStringView uri)
{
  return serd_a_token(SERD_CURIE, uri);
}

/**
   A CURIE node from a string.

   @param curie The CURIE string (a prefixed name separated with ':')
*/
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_curie_string(const char* ZIX_NONNULL curie)
{
  return serd_a_token(SERD_CURIE, zix_string(curie));
}

/**
   A literal node with a datatype.

   @param string The string body of the node.
   @param datatype The absolute URI of the datatype.
*/
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_typed_literal(const ZixStringView         string,
                     const SerdNode* ZIX_NONNULL datatype)
{
  return serd_a_literal(string, SERD_HAS_DATATYPE, datatype);
}

/**
   A literal node with a language.

   @param string The string body of the node.
   @param language A language tag like "en-ca".
*/
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_plain_literal(const ZixStringView         string,
                     const SerdNode* ZIX_NONNULL language)
{
  return serd_a_literal(string, SERD_HAS_LANGUAGE, language);
}

/**
   A canonical literal for a primitive value.

   The node will be a typed literal in canonical form for the xsd datatype
   corresponding to the value.
*/
SERD_CONST_API SerdNodeArgs
serd_a_primitive(SerdValue value);

/**
   A canonical xsd:decimal literal.

   The node will be an xsd:decimal literal, like "12.34", with datatype
   xsd:decimal.

   The node will always contain a '.', start with a digit, and end with a digit
   (a leading and/or trailing '0' will be added if necessary), for example,
   "1.0".  It will never be in scientific notation.
*/
SERD_CONST_API SerdNodeArgs
serd_a_decimal(double value);

/**
   A canonical xsd:integer literal.

   The node will be an xsd:integer literal like "1234", with datatype
   xsd:integer.
*/
SERD_CONST_API SerdNodeArgs
serd_a_integer(int64_t value);

/**
   A canonical xsd:hexBinary literal.

   The node will be an xsd:hexBinary literal like "534D", with datatype
   xsd:hexBinary.
*/
SERD_CONST_API SerdNodeArgs
serd_a_hex(size_t size, const void* ZIX_NONNULL data);

/**
   A canonical xsd:base64Binary literal.

   The node will be an xsd:base64Binary literal like "Zm9vYmFy", with datatype
   xsd:base64Binary.
*/
SERD_CONST_API SerdNodeArgs
serd_a_base64(size_t size, const void* ZIX_NONNULL data);

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
   buffer is too small for the node, then `status` will be #SERD_NO_SPACE, and
   `count` will be set to the number of bytes required to successfully
   construct the node.
*/
SERD_API SerdStreamResult
serd_node_construct(size_t buf_size, void* ZIX_NULLABLE buf, SerdNodeArgs args);

/**
   @}
   @defgroup serd_node_dynamic_allocation Dynamic Allocation

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
SERD_API SerdNode* ZIX_ALLOCATED
serd_node_new(ZixAllocator* ZIX_NULLABLE allocator, SerdNodeArgs args);

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

   @return The size of the decoded hex or base64 blob `node`, or zero if it
   does not have datatype <http://www.w3.org/2001/XMLSchema#hexBinary> or
   <http://www.w3.org/2001/XMLSchema#base64Binary>.
*/
SERD_PURE_API size_t
serd_node_decoded_size(const SerdNode* ZIX_NONNULL node);

/**
   Decode a binary (base64 or hex) node.

   This function can be used to decode a node created with serd_a_base64() or
   serd_a_hex() and retrieve the original unencoded binary data.

   @param node A literal node which is an encoded base64 or hex string.

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
