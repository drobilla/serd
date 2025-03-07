// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_ARGS_H
#define SERD_NODE_ARGS_H

#include <serd/attributes.h>
#include <serd/blob_view.h>
#include <serd/literal_view.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/string_pair_view.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <serd/value.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stddef.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_node_arguments Node Arguments

   A unified representation of the arguments needed to specify any node.

   Since there are many ways to describe a node, and many functions that take
   node descriptions as arguments, the arguments to specify a node are
   encapsulated in a single struct to prevent a combinatorial explosion of
   functions.

   For convenience, several constructors are provided that return a temporary
   view of their arguments, which can be passed (usually inline) to such
   functions.

   @ingroup serd_data
   @{
*/

/**
   Macro for writing portable struct literals.

   This is used by the implementation of the construction API, but isn't of
   much use to client code, which can either use that, or use struct
   initalization syntax directly. The macro is needed because C and C++
   annoyingly have incompatible syntax here.
*/
#ifdef __cplusplus
#  define SERD_NODE_ARGS(type, ...) \
    SerdNodeArgs                    \
    {                               \
      SERD_NODE_ARGS_##type,        \
      {                             \
        __VA_ARGS__                 \
      }                             \
    }
#else
#  define SERD_NODE_ARGS(type, ...) \
    (SerdNodeArgs)                  \
    {                               \
      SERD_NODE_ARGS_##type,        \
      {                             \
        __VA_ARGS__                 \
      }                             \
    }
#endif

/// The type of a #SerdNodeArgs
typedef enum {
  SERD_NODE_ARGS_NODE_ID,       ///< Interned node ID @see serd_a_node_id
  SERD_NODE_ARGS_URI,           ///< Parsed URI @see serd_a_uri_view
  SERD_NODE_ARGS_HOST_PATH,     ///< File URI @see serd_a_path
  SERD_NODE_ARGS_PREFIXED_NAME, ///< Prefixed name @see serd_a_prefixed_name
  SERD_NODE_ARGS_JOINED_URI,    ///< Joined URI @see serd_a_joined_uri
  SERD_NODE_ARGS_TOKEN,         ///< Token @see serd_a_token
  SERD_NODE_ARGS_OBJECT,        ///< Object @see serd_a_object
  SERD_NODE_ARGS_LITERAL,       ///< Literal @see serd_a_literal
  SERD_NODE_ARGS_VALUE,         ///< Primitive number @see serd_a_value
  SERD_NODE_ARGS_DECIMAL,       ///< Decimal number @see serd_a_decimal
  SERD_NODE_ARGS_INTEGER,       ///< Integer number @see serd_a_integer
  SERD_NODE_ARGS_HEX,           ///< Hex-encoded blob @see serd_a_hex
  SERD_NODE_ARGS_BASE64,        ///< Base64-encoded blob @see serd_a_base64
} SerdNodeArgsType;

/// The data of a #SerdNodeArgs
typedef union {
  /// Data for #SERD_NODE_ARGS_NODE_ID
  SerdNodeID node_id;

  /// Data for #SERD_NODE_ARGS_URI
  SerdURIView uri;

  /**
     Data for #SERD_NODE_ARGS_HOST_PATH, #SERD_NODE_ARGS_PREFIXED_NAME, and
     #SERD_NODE_ARGS_JOINED_URI.
  */
  SerdStringPairView string_pair;

  /// Data for #SERD_NODE_ARGS_TOKEN
  SerdTokenView token;

  /// Data for #SERD_NODE_ARGS_OBJECT
  SerdObjectView object;

  /// Data for #SERD_NODE_ARGS_LITERAL
  SerdLiteralView literal;

  /**
     Data for #SERD_NODE_ARGS_VALUE, #SERD_NODE_ARGS_DECIMAL, and
     #SERD_NODE_ARGS_INTEGER.
  */
  SerdValue value;

  /// Data for #SERD_NODE_ARGS_HEX and #SERD_NODE_ARGS_BASE64
  SerdBlobView blob;
} SerdNodeArgsData;

/// Arguments for constructing a node
typedef struct {
  SerdNodeArgsType type; ///< Type of node described and valid field of `data`
  SerdNodeArgsData data; ///< Data union
} SerdNodeArgs;

/// A wildcard or other sentinel node (a token with type #SERD_NOTHING)
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_null(void)
{
  return SERD_NODE_ARGS(NODE_ID, 0U);
}

/// An interned node ID
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_node_id(const SerdNodeID id)
{
  return SERD_NODE_ARGS(NODE_ID, id);
}

/// A URI node from a parsed URI
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_uri_view(const SerdURIView uri)
{
  return SERD_NODE_ARGS(URI, .uri = uri);
}

/// A file URI node from a path and optional hostname
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_path(const ZixStringView path, const ZixStringView hostname)
{
  return SERD_NODE_ARGS(HOST_PATH, .string_pair = {hostname, path});
}

/// A CURIE node from a prefix name and a local name (an abbreviated URI)
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_prefixed_name(const ZixStringView prefix, const ZixStringView name)
{
  return SERD_NODE_ARGS(PREFIXED_NAME, .string_pair = {prefix, name});
}

/// A URI node from a joined prefix and suffix (an in-place expanded CURIE)
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_joined_uri(const ZixStringView prefix, const ZixStringView suffix)
{
  return SERD_NODE_ARGS(JOINED_URI, .string_pair = {prefix, suffix});
}

/**
   A simple "token" node.

   "Token" is just a shorthand used in this API to refer to a node that isn't
   a typed or tagged literal.  This
   can be used to create URIs, blanko nodes, variables, and simple string
   literals.

   Note that string literals constructed with this function will have no flags
   set, and so will be written as "short" literals (not triple-quoted).  To
   construct long literals, use the more advanced serd_a_object() with the
   #SERD_IS_LONG flag.
*/
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_token(const SerdNodeType type, const ZixStringView string)
{
  return SERD_NODE_ARGS(TOKEN, .token = {type, string});
}

/**
   An object node.

   The object of a statement is a special field that can be more complex than
   the other simple "token" fields.  In addition to the usual node type and
   string, it has a set of flags, and a "meta" string that is either a language
   tag or datatype URI (as dictated by the flags).
*/
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_object(const SerdNodeType  type,
              const ZixStringView string,
              const SerdNodeFlags flags,
              const SerdTokenView meta)
{
  return SERD_NODE_ARGS(OBJECT, .object = {type, string, flags, meta});
}

/// A token node from a token view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_token_view(const SerdTokenView token)
{
  return serd_a_token(token.type, token.string);
}

/// An object node from a object view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_object_view(const SerdObjectView object)
{
  return serd_a_object(object.type, object.string, object.flags, object.meta);
}

/// A simple string literal node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_string(const ZixStringView string)
{
  return serd_a_token(SERD_LITERAL, string);
}

/// A blank node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_blank(const ZixStringView name)
{
  return serd_a_token(SERD_BLANK, name);
}

/// A URI node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_uri(const ZixStringView uri)
{
  return serd_a_token(SERD_URI, uri);
}

/// A CURIE node from a string view
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_curie(ZixStringView uri)
{
  return serd_a_token(SERD_CURIE, uri);
}

/**
   A literal node with an optional datatype or language.

   Either a datatype (which must be an absolute URI) or a language (which must
   be an RFC5646 language tag) may be given, but not both.

   This is the most general literal constructor, which can be used to construct
   any literal node.

   @param string The string body of the node.

   @param flags Flags that describe the details of the node.

   @param meta If #SERD_HAS_DATATYPE is set, then this must be the ID of an
   absolute datatype URI.  If #SERD_HAS_LANGUAGE is set, then this must be the
   ID of a language tag like "en-ca".  Otherwise, it is ignored.
*/
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_literal(const ZixStringView string,
               const SerdNodeFlags flags,
               const SerdNodeID    meta)
{
  return SERD_NODE_ARGS(LITERAL, .literal = {string, flags, meta});
}

/// A literal node with a datatype URI
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_typed_literal(const ZixStringView string, const ZixStringView datatype)
{
  return serd_a_object(SERD_LITERAL,
                       string,
                       SERD_HAS_DATATYPE,
                       serd_token_view(SERD_URI, datatype));
}

/// A literal node with a language tag
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_plain_literal(const ZixStringView string, const ZixStringView language)
{
  return serd_a_object(SERD_LITERAL,
                       string,
                       SERD_HAS_LANGUAGE,
                       serd_token_view(SERD_LITERAL, language));
}

/// A canonical literal for a primitive value
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_value(const SerdValue value)
{
  return SERD_NODE_ARGS(VALUE, .value = value);
}

/// A canonical xsd:decimal literal
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_decimal(const double value)
{
  return SERD_NODE_ARGS(DECIMAL, .value = serd_double(value));
}

/// A canonical xsd:integer literal
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_integer(const int64_t value)
{
  return SERD_NODE_ARGS(INTEGER, .value = serd_long(value));
}

/// A canonical xsd:hexBinary literal
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_hex(const size_t size, const void* ZIX_NONNULL const data)
{
  return SERD_NODE_ARGS(HEX, .blob = {size, data});
}

/// A canonical xsd:base64Binary literal
ZIX_CONST_FUNC static inline SerdNodeArgs
serd_a_base64(const size_t size, const void* ZIX_NONNULL const data)
{
  return SERD_NODE_ARGS(BASE64, .blob = {size, data});
}

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_ARGS_H
