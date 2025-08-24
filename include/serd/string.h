// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_H
#define SERD_STRING_H

#include <serd/attributes.h>
#include <serd/node_flags.h>
#include <serd/struct_literal.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_string String Utilities
   @ingroup serd_utilities
   @{
*/

/// A mutable string with a length
typedef struct {
  size_t                length; ///< Length in bytes without terminator
  char* ZIX_UNSPECIFIED data;   ///< Buffer
} SerdString;

/**
   Return a view of a string.

   This is a syntactic convenience for conversion to a ZixStringView,
   particularly when passing a string parameter.
*/
ZIX_CONST_FUNC static inline ZixStringView
serd_string_view(const SerdString string)
{
  return SERD_STRUCT_LITERAL(
    ZixStringView, string.data ? string.data : "", string.length);
}

/**
   Allocate a new string with the given contents.

   @param allocator Allocator for the returned string data.
   @param contents The contents to copy to the new string.
   @return A string with newly-allocated, or NULL, `data`.
*/
SERD_API SerdString
serd_string_new(ZixAllocator* ZIX_NULLABLE allocator, ZixStringView contents);

/**
   Measure a UTF-8 string.

   @return Length of `str` in bytes.
   @param str A null-terminated UTF-8 string.
   @param flags (Output) Set to the applicable flags.
*/
SERD_API size_t
serd_strlen(const char* ZIX_NONNULL str, SerdNodeFlags* ZIX_NULLABLE flags);

/**
   Parse a string to a double.

   The API of this function is identical to the standard C strtod function,
   except this function is locale-independent and always matches the lexical
   format used in the Turtle grammar (the decimal point is always ".").
*/
SERD_API double
serd_strtod(const char* ZIX_NONNULL             str,
            char* ZIX_UNSPECIFIED* ZIX_NULLABLE endptr);

/**
   Decode a base64 string.

   @param allocator Allocator for the returned blob.
   @param str Base64 string to decode.
   @param len The length of `str`.
   @param size Set to the size of the returned blob in bytes.
   @return A newly allocated blob which must be freed with serd_free().
*/
SERD_API void* ZIX_ALLOCATED
serd_base64_decode(ZixAllocator* ZIX_NULLABLE allocator,
                   const char* ZIX_NONNULL    str,
                   size_t                     len,
                   size_t* ZIX_NONNULL        size);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_H
