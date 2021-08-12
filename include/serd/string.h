// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_H
#define SERD_STRING_H

#include "serd/attributes.h"
#include "serd/node.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_string String Utilities
   @ingroup serd
   @{
*/

/**
   Compare two strings ignoring case.

   @return Less than, equal to, or greater than zero if `s1` is less than,
   equal to, or greater than `s2`, respectively.
*/
SERD_PURE_API
int
serd_strncasecmp(const char* SERD_NONNULL s1,
                 const char* SERD_NONNULL s2,
                 size_t                   n);

/**
   Decode a base64 string.

   This function can be used to decode a node created with serd_new_base64().

   @param str Base64 string to decode.
   @param len The length of `str`.
   @param size Set to the size of the returned blob in bytes.
   @return A newly allocated blob which must be freed with serd_free().
*/
SERD_API
void* SERD_ALLOCATED
serd_base64_decode(const char* SERD_NONNULL str,
                   size_t                   len,
                   size_t* SERD_NONNULL     size);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_H
