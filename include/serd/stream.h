// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STREAM_H
#define SERD_STREAM_H

#include "serd/attributes.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "zix/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_stream Byte Stream Interface
   @ingroup serd_reading_writing

   These types define the interface for byte streams (generalized files) which
   can be provided to read/write from/to any custom source/sink.  Wrappers are
   provided to easily create streams for a standard C `FILE`.

   @{
*/

/**
   Function for detecting I/O stream errors.

   Identical semantics to `ferror`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdErrorFunc)(void* ZIX_UNSPECIFIED stream);

/**
   Function for closing an I/O stream.

   Identical semantics to `fclose`.  Note that when writing, this may flush the
   stream which can cause errors, including errors caused by previous writes
   that appeared successful at the time.  Therefore it is necessary to check
   the return value of this function to properly detect write errors.

   @return Non-zero if `stream` has encountered an error.
*/
typedef SerdStatus (*SerdCloseFunc)(void* ZIX_UNSPECIFIED stream);

/**
   Function for reading input bytes from a stream.

   @param stream Stream to read from.
   @param len Number of bytes to read.
   @param buf Output buffer.
   @return Number of bytes read (which is short on error), and a status code.
*/
typedef SerdStreamResult (*SerdReadFunc)(void* ZIX_UNSPECIFIED stream,
                                         size_t                len,
                                         void* ZIX_NONNULL     buf);

/**
   Function for writing output bytes to a stream with status.

   @param stream Stream to write to.
   @param len Number of bytes to write.
   @param buf Input buffer.

   @return Number of bytes written (which is short on error), and a status code.
*/
typedef SerdStreamResult (*SerdWriteFunc)(void* ZIX_UNSPECIFIED   stream,
                                          size_t                  len,
                                          const void* ZIX_NONNULL buf);

/**
   Wrapper for fread() that is a SerdReadFunc.

   This can be used to easily create an input stream for a FILE.
*/
SERD_API SerdStreamResult
serd_fread_wrapper(void* ZIX_NONNULL stream, size_t len, void* ZIX_NONNULL buf);

/**
   Wrapper for fwrite() that is a SerdWriteFunc.

   This can be used to easily create an output stream for a FILE.
*/
SERD_API SerdStreamResult
serd_fwrite_wrapper(void* ZIX_NONNULL       stream,
                    size_t                  len,
                    const void* ZIX_NONNULL buf);

/**
   Wrapper for fclose() that is a SerdCloseFunc.

   This can be used in conjunction with serd_fread_wrapper() or
   serd_fwrite_wrapper() to create a stream for a FILE that should have
   fclose() called on it when the stream is closed.
*/
SERD_API SerdStatus
serd_fclose_wrapper(void* ZIX_NONNULL stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STREAM_H
