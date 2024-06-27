// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BUFFER_H
#define SERD_BUFFER_H

#include "serd/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_buffer Writable Buffers
   @ingroup serd_memory

   The #SerdBuffer type represents a writable area of memory with a known size.

   @{
*/

/// A mutable buffer in memory
typedef struct {
  void* SERD_NULLABLE buf; ///< Buffer
  size_t              len; ///< Size of buffer in bytes
} SerdBuffer;

/**
   A convenience sink function for writing to a string.

   This function can be used as a #SerdWriteFunc to write to a SerdBuffer which
   is resized as necessary with realloc().  The `stream` parameter must point
   to an initialized #SerdBuffer.  When the write is finished, the string
   should be retrieved with serd_buffer_sink_finish().
*/
SERD_API size_t
serd_buffer_sink(const void* SERD_NONNULL buf,
                 size_t                   len,
                 void* SERD_UNSPECIFIED   stream);

/**
   Finish writing to a buffer with serd_buffer_sink().

   The returned string is the result of the serialisation, which is null
   terminated (by this function) and owned by the caller.
*/
SERD_API char* SERD_NONNULL
serd_buffer_sink_finish(SerdBuffer* SERD_NONNULL stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BUFFER_H
