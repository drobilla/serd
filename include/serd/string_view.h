// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_VIEW_H
#define SERD_STRING_VIEW_H

#include "serd/attributes.h"

#include <stddef.h>
#include <string.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_string_view String View
   @ingroup serd_utilities
   @{
*/

/**
   An immutable slice of a string.

   This type is used for many string parameters, to allow referring to slices
   of strings in-place and to avoid redundant string measurement.
*/
typedef struct {
  const char* SERD_NONNULL data;   ///< Start of string
  size_t                   length; ///< Length of string in bytes
} SerdStringView;

/// Return a view of an empty string
SERD_ALWAYS_INLINE_FUNC SERD_CONST_FUNC static inline SerdStringView
serd_empty_string(void)
{
  const SerdStringView view = {"", 0U};
  return view;
}

/**
   Return a view of a substring, or a premeasured string.

   This makes either a view of a slice of a string (which may not be null
   terminated), or a view of a string that has already been measured.  This is
   faster than serd_string() for dynamic strings since it does not call
   `strlen`, so should be used when the length of the string is already known.

   @param str Pointer to the start of the substring.

   @param len Length of the substring in bytes, not including the trailing null
   terminator if present.
*/
SERD_ALWAYS_INLINE_FUNC SERD_CONST_FUNC static inline SerdStringView
serd_substring(const char* const SERD_NONNULL str, const size_t len)
{
  const SerdStringView view = {str, len};
  return view;
}

/**
   Return a view of an entire string by measuring it.

   This makes a view of the given string by measuring it with `strlen`.

   @param str Pointer to the start of a null-terminated C string, or null.
*/
SERD_ALWAYS_INLINE_FUNC SERD_PURE_FUNC static inline SerdStringView
// NOLINTNEXTLINE(clang-diagnostic-unused-function)
serd_string(const char* const SERD_NULLABLE str)
{
  return str ? serd_substring(str, strlen(str)) : serd_empty_string();
}

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_VIEW_H
