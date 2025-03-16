// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BUFFER_H
#define SERD_BUFFER_H

#include <serd/attributes.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_buffer Writable Buffers
   @ingroup serd_memory

   The #SerdBuffer type represents a writable area of memory with a known size.

   @{
*/

/// A dynamically resizable mutable string in memory
typedef struct {
  ZixAllocator* ZIX_NULLABLE allocator; ///< Allocator for buf
  char* ZIX_NULLABLE         buf;       ///< Buffer
  size_t                     len;       ///< Size of buffer in bytes
} SerdBuffer;

/**
   A convenience sink function for writing to a string buffer.

   This function can be used as a #SerdWriteFunc to write to a SerdBuffer which
   is resized as necessary with realloc().  The `stream` parameter must point
   to an initialized #SerdBuffer.  When the write is finished, the string
   should be retrieved with serd_buffer_sink_finish().
*/
SERD_API size_t
serd_buffer_sink(const void* ZIX_NONNULL buf,
                 size_t                  len,
                 void* ZIX_UNSPECIFIED   stream);

/**
   Finish writing to a buffer with serd_buffer_sink().

   @return The buffer as a null-terminated string, or null on allocation
   failure or any other error.
*/
SERD_API char* ZIX_ALLOCATED
serd_buffer_sink_finish(SerdBuffer* ZIX_NONNULL stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BUFFER_H
