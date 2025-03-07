// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BINARY_H
#define SERD_BINARY_H

#include <serd/attributes.h>
#include <serd/object_view.h>
#include <serd/stream_result.h>
#include <zix/attributes.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_binary Binary
   @ingroup serd_data
   @{
*/

/**
   Return the maximum size of a decoded binary node in bytes.

   This returns an upper bound on the number of bytes that the node would
   decode to.  This is calculated as a simple constant-time arithmetic
   expression based on the length of the encoded string, so may be larger than
   the actual size of the data due to things like additional whitespace.

   @param node View of a literal which is an encoded base64 or hex string.

   @return The maximum size of the decoded binary data, or zero if the node
   doesn't have datatype <http://www.w3.org/2001/XMLSchema#hexBinary> or
   <http://www.w3.org/2001/XMLSchema#base64Binary>.
*/
SERD_PURE_API size_t
serd_binary_decoded_size(SerdObjectView node);

/**
   Decode a base64 or hex binary node.

   @param node View of a literal which is an encoded base64 or hex string.

   @param buf_size The size of `buf` in bytes.

   @param buf Buffer where decoded data will be written.

   @return On success, #SERD_SUCCESS is returned along with the number of bytes
   written.  If the output buffer is too small, then #SERD_NO_SPACE is returned
   along with the number of bytes required for successful decoding.
*/
SERD_API SerdStreamResult
serd_binary_decode(SerdObjectView node, size_t buf_size, void* ZIX_NONNULL buf);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BINARY_H
