// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_H
#define SERD_STRING_H

#include "serd/attributes.h"
#include "zix/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_string String Utilities
   @ingroup serd_utilities
   @{
*/

/**
   Decode a base64 string.

   This function can be used to decode a node created with serd_new_base64().

   @param str Base64 string to decode.
   @param len The length of `str`.
   @param size Set to the size of the returned blob in bytes.
   @return A newly allocated blob which must be freed with zix_free().
*/
SERD_API void* ZIX_ALLOCATED
serd_base64_decode(const char* ZIX_NONNULL str,
                   size_t                  len,
                   size_t* ZIX_NONNULL     size);

/**
   Compare two strings ignoring case.

   @return Less than, equal to, or greater than zero if `s1` is less than,
   equal to, or greater than `s2`, respectively, ignoring case.
*/
SERD_PURE_API int
serd_strncasecmp(const char* ZIX_NONNULL s1,
                 const char* ZIX_NONNULL s2,
                 size_t                  n);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_H
