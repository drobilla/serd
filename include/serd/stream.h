// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STREAM_H
#define SERD_STREAM_H

#include "serd/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_stream Byte Stream Interface
   @ingroup serd

   These types define the interface for byte streams (generalized files) which
   can be provided to read/write from/to any custom source/sink.  It is
   directly compatible with the standard C `FILE` API, so the standard library
   functions may be used directly.

   @{
*/

/**
   Function to detect I/O stream errors.

   Identical semantics to `ferror`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamErrorFunc)(void* SERD_NONNULL stream);

/**
   Function to close an I/O stream.

   Identical semantics to `fclose`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamCloseFunc)(void* SERD_NONNULL stream);

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
   @}
*/

SERD_END_DECLS

#endif // SERD_STREAM_H
