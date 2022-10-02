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

/**
   Function to detect I/O stream errors.

   Identical semantics to `ferror`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamErrorFunc)(void* SERD_NONNULL stream);

/**
   Source function for raw string input.

   Identical semantics to `fread`, but may set errno for more informative error
   reporting than supported by SerdStreamErrorFunc.

   @param buf Output buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to read from (FILE* for fread).
   @return Number of elements (bytes) read.
*/
typedef size_t (*SerdSource)(void* SERD_NONNULL buf,
                             size_t             size,
                             size_t             nmemb,
                             void* SERD_NONNULL stream);

/// Sink function for raw string output
typedef size_t (*SerdSink)(const void* SERD_NONNULL buf,
                           size_t                   len,
                           void* SERD_NONNULL       stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STREAM_H
