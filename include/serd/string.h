// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_H
#define SERD_STRING_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "zix/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_string String Utilities
   @ingroup serd_utilities
   @{
*/

/**
   Measure a UTF-8 string.

   @return Length of `str` in bytes.
   @param str A null-terminated UTF-8 string.
   @param flags (Output) Set to the applicable flags.
*/
SERD_API size_t
serd_strlen(const char* ZIX_NONNULL str, SerdNodeFlags* ZIX_NULLABLE flags);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_H
