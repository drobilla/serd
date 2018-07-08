// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STREAM_H
#define SERD_STREAM_H

#include "serd/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_streams Byte Streams
   @ingroup serd
   @{
*/

/// A sink for bytes that receives text output
typedef struct SerdByteSinkImpl SerdByteSink;

/**
   Function to detect I/O stream errors.

   Identical semantics to `ferror`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamErrorFunc)(void* SERD_NONNULL stream);

/**
   Function for reading input bytes from a stream.

   This has identical semantics to `fread`, but may set `errno` for more
   informative error reporting than supported by #SerdStreamErrorFunc.

   @param buf Output buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to read from (FILE* for fread).
   @return Number of elements (bytes) read, which is short on error.
*/
typedef size_t (*SerdReadFunc)(void* SERD_NONNULL buf,
                               size_t             size,
                               size_t             nmemb,
                               void* SERD_NONNULL stream);

/**
   Function for writing output bytes to a stream.

   This has identical semantics to `fwrite`, but may set `errno` for more
   informative error reporting than supported by #SerdStreamErrorFunc.

   @param buf Input buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to write to (FILE* for fread).
   @return Number of elements (bytes) written, which is short on error.
*/
typedef size_t (*SerdWriteFunc)(const void* SERD_NONNULL buf,
                                size_t                   size,
                                size_t                   nmemb,
                                void* SERD_NONNULL       stream);

/**
   Create a new byte sink.

   @param write_func Function called with bytes to consume.
   @param stream Context parameter passed to `sink`.
   @param block_size Number of bytes to write per call.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new(SerdWriteFunc SERD_NONNULL write_func,
                   void* SERD_NULLABLE        stream,
                   size_t                     block_size);

/**
   Write to `sink`.

   Compatible with SerdWriteFunc.
*/
SERD_API
size_t
serd_byte_sink_write(const void* SERD_NONNULL   buf,
                     size_t                     size,
                     size_t                     nmemb,
                     SerdByteSink* SERD_NONNULL sink);

/// Flush any pending output in `sink` to the underlying write function
SERD_API
void
serd_byte_sink_flush(SerdByteSink* SERD_NONNULL sink);

/// Free `sink`
SERD_API
void
serd_byte_sink_free(SerdByteSink* SERD_NULLABLE sink);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STREAM_H
