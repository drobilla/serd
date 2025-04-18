// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_H
#define SERD_STRING_H

#include <serd/attributes.h>
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
   Compare two strings ignoring case.

   @return Less than, equal to, or greater than zero if `lhs` is less than,
   equal to, or greater than `rhs`, respectively, ignoring case.
*/
SERD_PURE_API int
serd_strcasecmp(const char* ZIX_NONNULL lhs, const char* ZIX_NONNULL rhs);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_H
