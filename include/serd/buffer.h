// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BUFFER_H
#define SERD_BUFFER_H

#include "serd/attributes.h"
#include "serd/memory.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_buffer Dynamic Memory Buffers
   @ingroup serd

   The #SerdBuffer type represents a writable area of memory with a known size.
   An implementation of #SerdWriteFunc, #SerdErrorFunc, and #SerdCloseFunc are
   provided which allow output to be written to a buffer in memory instead of
   to a file as with `fwrite`, `ferror`, and `fclose`.

   @{
*/

/// A dynamically resizable mutable buffer in memory
typedef struct {
  SerdAllocator* SERD_NULLABLE allocator; ///< Allocator for buf
  void* SERD_NULLABLE          buf;       ///< Buffer
  size_t                       len;       ///< Size of buffer in bytes
} SerdBuffer;

/**
   A function for writing to a buffer, resizing it if necessary.

   This function can be used as a #SerdWriteFunc to write to a #SerdBuffer
   which is reallocated as necessary.  The `stream` parameter must point to an
   initialized #SerdBuffer.

   Note that when writing a string, the string in the buffer will not be
   null-terminated until serd_buffer_close() is called.
*/
SERD_API
size_t
serd_buffer_write(const void* SERD_NONNULL buf,
                  size_t                   size,
                  size_t                   nmemb,
                  void* SERD_NONNULL       stream);

/**
   Close the buffer for writing.

   This writes a terminating null byte, so the contents of the buffer are safe
   to read as a string after this call.
*/
SERD_API
int
serd_buffer_close(void* SERD_NONNULL stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BUFFER_H
