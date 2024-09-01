// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_H
#define SERD_NODE_H

#include <serd/attributes.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/uri.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_node Node
   @ingroup serd_data
   @{
*/

/// A syntactic RDF node
typedef struct {
  const char* ZIX_NULLABLE buf;     ///< Value string
  size_t                   n_bytes; ///< Size in bytes (excluding null)
  SerdNodeFlags            flags;   ///< Node flags (string properties)
  SerdNodeType             type;    ///< Node type
} SerdNode;

static const SerdNode SERD_NODE_NULL = {NULL, 0, 0, SERD_NOTHING};

/**
   Make a (shallow) node from `str`.

   This measures, but does not copy, `str`.  No memory is allocated.
*/
SERD_API SerdNode
serd_node_from_string(SerdNodeType type, const char* ZIX_NULLABLE str);

/**
   Make a (shallow) node from a prefix of `str`.

   This measures, but does not copy, `str`.  No memory is allocated.
   Note that the returned node may not be null terminated.
*/
SERD_API SerdNode
serd_node_from_substring(SerdNodeType             type,
                         const char* ZIX_NULLABLE str,
                         size_t                   len);

/// Simple wrapper for serd_node_new_uri() to resolve a URI node
SERD_API SerdNode
serd_node_new_uri_from_node(ZixAllocator* ZIX_NULLABLE  allocator,
                            const SerdNode* ZIX_NONNULL uri_node);

/// Simple wrapper for serd_node_new_uri() to resolve a URI string
SERD_API SerdNode
serd_node_new_uri_from_string(ZixAllocator* ZIX_NULLABLE allocator,
                              const char* ZIX_NULLABLE   str);

/**
   Create a new file URI node from a file system path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.
*/
SERD_API SerdNode
serd_node_new_file_uri(ZixAllocator* ZIX_NULLABLE allocator,
                       const char* ZIX_NONNULL    path,
                       const char* ZIX_NULLABLE   hostname);

/**
   Create a new node by serialising `uri` into a new string.

   @param allocator Allocator for the returned node.

   @param uri The URI to serialise.
*/
SERD_API SerdNode
serd_node_new_uri(ZixAllocator* ZIX_NULLABLE     allocator,
                  const SerdURIView* ZIX_NONNULL uri);

/**
   Create a new node by serialising `d` into an xsd:decimal string.

   The resulting node will always contain a '.', start with a digit, and end
   with a digit (i.e. will have a leading and/or trailing '0' if necessary).
   It will never be in scientific notation.  A maximum of `frac_digits` digits
   will be written after the decimal point, but trailing zeros will
   automatically be omitted (except one if `d` is a round integer).

   Note that about 16 and 8 fractional digits are required to precisely
   represent a double and float, respectively.

   @param allocator Allocator for the returned node.
   @param d The value for the new node.
   @param frac_digits The maximum number of digits after the decimal place.
*/
SERD_API SerdNode
serd_node_new_decimal(ZixAllocator* ZIX_NULLABLE allocator,
                      double                     d,
                      unsigned                   frac_digits);

/// Create a new node by serialising `i` into an xsd:integer string
SERD_API SerdNode
serd_node_new_integer(ZixAllocator* ZIX_NULLABLE allocator, int64_t i);

/**
   Create a node by serialising `buf` into an xsd:base64Binary string.

   This function can be used to make a serialisable node out of arbitrary
   binary data, which can be decoded using serd_base64_decode().

   @param allocator Allocator for the returned node.
   @param buf Raw binary input data.
   @param size Size of `buf`.
   @param wrap_lines Wrap lines at 76 characters to conform to RFC 2045.
*/
SERD_API SerdNode
serd_node_new_blob(ZixAllocator* ZIX_NULLABLE allocator,
                   const void* ZIX_NONNULL    buf,
                   size_t                     size,
                   bool                       wrap_lines);

/**
   Make a deep copy of `node`.

   @return a node that the caller must free with serd_node_free().
*/
SERD_API SerdNode
serd_node_copy(ZixAllocator* ZIX_NULLABLE   allocator,
               const SerdNode* ZIX_NULLABLE node);

/// Return true iff `a` is equal to `b`
SERD_PURE_API bool
serd_node_equals(const SerdNode* ZIX_NONNULL a, const SerdNode* ZIX_NONNULL b);

/**
   Free any data owned by `node`.

   Note that if `node` is itself dynamically allocated (which is not the case
   for nodes created internally by serd), it will not be freed.
*/
SERD_API void
serd_node_free(ZixAllocator* ZIX_NULLABLE allocator,
               SerdNode* ZIX_NULLABLE     node);

/// Return a view of the string in a node
SERD_PURE_API ZixStringView
serd_node_string_view(const SerdNode* ZIX_NONNULL node);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_H
