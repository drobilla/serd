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
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_H
