// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_VIEW_H
#define SERD_STRING_VIEW_H

#include "serd/attributes.h"

#include <stddef.h>

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

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_VIEW_H
