// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BUFFER_H
#define SERD_BUFFER_H

#include "serd/attributes.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_buffer Writable Buffers
   @ingroup serd_memory

   The #SerdBuffer type represents a writable area of memory with a known size.

   #SerdWriteFunc and #SerdCloseFunc functions are provided which enable
   writing output to a memory buffer (as `fwrite` and `fclose` do for files).

   @{
*/

/// A dynamically resizable mutable buffer in memory
typedef struct {
  ZixAllocator* ZIX_NULLABLE allocator; ///< Allocator for buf
  void* ZIX_NULLABLE         buf;       ///< Buffer
  size_t                     len;       ///< Size of buffer in bytes
} SerdBuffer;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BUFFER_H
