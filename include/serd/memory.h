// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_MEMORY_H
#define SERD_MEMORY_H

#include "serd/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_memory Memory Management
   @ingroup serd
   @{
*/

struct SerdAllocatorImpl; // Defined below

/**
   A memory allocator.

   This object-like structure provides an interface like the standard C
   functions malloc(), calloc(), realloc(), free(), and aligned_alloc().  It
   contains function pointers that differ from their standard counterparts by
   taking a context parameter (a pointer to this struct), which allows the user
   to implement custom stateful allocators.
*/
typedef struct SerdAllocatorImpl SerdAllocator;

/**
   General malloc-like memory allocation function.

   This works like the standard C malloc(), except has an additional handle
   parameter for implementing stateful allocators without static data.
*/
typedef void* SERD_ALLOCATED (*SerdAllocatorMallocFunc)( //
  SerdAllocator* SERD_NULLABLE allocator,
  size_t                       size);

/**
   General calloc-like memory allocation function.

   This works like the standard C calloc(), except has an additional handle
   parameter for implementing stateful allocators without static data.
*/
typedef void* SERD_ALLOCATED (*SerdAllocatorCallocFunc)( //
  SerdAllocator* SERD_NULLABLE allocator,
  size_t                       nmemb,
  size_t                       size);

/**
   General realloc-like memory reallocation function.

   This works like the standard C remalloc(), except has an additional handle
   parameter for implementing stateful allocators without static data.
*/
typedef void* SERD_ALLOCATED (*SerdAllocatorReallocFunc)( //
  SerdAllocator* SERD_NULLABLE allocator,
  void* SERD_NULLABLE          ptr,
  size_t                       size);

/**
   General free-like memory deallocation function.

   This works like the standard C remalloc(), except has an additional handle
   parameter for implementing stateful allocators without static data.
*/
typedef void (*SerdAllocatorFreeFunc)( //
  SerdAllocator* SERD_NULLABLE allocator,
  void* SERD_NULLABLE          ptr);

/**
   General aligned_alloc-like memory deallocation function.

   This works like the standard C aligned_alloc(), except has an additional
   handle parameter for implementing stateful allocators without static data.
*/
typedef void* SERD_ALLOCATED (*SerdAllocatorAlignedAllocFunc)( //
  SerdAllocator* SERD_NULLABLE allocator,
  size_t                       alignment,
  size_t                       size);

/**
   General aligned memory deallocation function.

   This works like the standard C free(), but must be used to free memory
   allocated with the aligned_alloc() method of the allocator.  This allows
   portability to systems (like Windows) that can not use the same free function
   in these cases.
*/
typedef void (*SerdAllocatorAlignedFreeFunc)( //
  SerdAllocator* SERD_NULLABLE allocator,
  void* SERD_NULLABLE          ptr);

/// Definition of SerdAllocator
struct SerdAllocatorImpl {
  SerdAllocatorMallocFunc SERD_ALLOCATED       malloc;
  SerdAllocatorCallocFunc SERD_ALLOCATED       calloc;
  SerdAllocatorReallocFunc SERD_ALLOCATED      realloc;
  SerdAllocatorFreeFunc SERD_ALLOCATED         free;
  SerdAllocatorAlignedAllocFunc SERD_ALLOCATED aligned_alloc;
  SerdAllocatorAlignedFreeFunc SERD_ALLOCATED  aligned_free;
};

/// Return the default allocator which simply uses the system allocator
SERD_CONST_API
SerdAllocator* SERD_NONNULL
serd_default_allocator(void);

/**
   Free memory allocated by Serd.

   This function exists because some systems require memory allocated by a
   library to be freed by code in the same library.  It is otherwise equivalent
   to the standard C free() function.

   This may be used to free memory allocated using serd_default_allocator().
*/
SERD_API
void
serd_free(SerdAllocator* SERD_NULLABLE allocator, void* SERD_NULLABLE ptr);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_MEMORY_H
