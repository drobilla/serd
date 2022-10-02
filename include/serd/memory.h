// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_MEMORY_H
#define SERD_MEMORY_H

#include "serd/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_allocator Allocator
   @ingroup serd_memory
   @{
*/

/**
   Free memory allocated by Serd.

   This function exists because some systems require memory allocated by a
   library to be freed by code in the same library.  It is otherwise equivalent
   to the standard C free() function.
*/
SERD_API void
serd_free(void* SERD_NULLABLE ptr);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_MEMORY_H
