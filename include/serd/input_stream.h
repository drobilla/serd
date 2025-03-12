// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_INPUT_STREAM_H
#define SERD_INPUT_STREAM_H

#include <serd/attributes.h>
#include <serd/status.h>
#include <serd/stream.h>
#include <zix/attributes.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_input_stream Input Streams
   @ingroup serd_reading_writing
   @{
*/

/**
   An input stream of bytes.

   This is a general abstraction for a byte source, for example a UTF-8 text
   file opened for reading.
*/
typedef struct {
  void* ZIX_UNSPECIFIED      stream; ///< Opaque stream for `read` and `close`
  SerdReadFunc ZIX_NONNULL   read;   ///< Read bytes from `stream`
  SerdCloseFunc ZIX_NULLABLE close;  ///< Close `stream`
} SerdInputStream;

/**
   Open a stream that reads from a provided function.

   @param read_func Function to read bytes from the stream.
   @param close_func Function to close the stream.
   @param stream Stream parameter passed to `read_func` and `close_func`.
   @return An opened input stream, or all zeros on error.
*/
SERD_CONST_API SerdInputStream
serd_open_input_stream(SerdReadFunc ZIX_NONNULL   read_func,
                       SerdCloseFunc ZIX_NULLABLE close_func,
                       void* ZIX_UNSPECIFIED      stream);

/**
   Open a stream that reads from a string.

   The string pointer that `position` points to must remain valid until the
   stream is closed.  This pointer serves as the internal stream state and will
   be mutated as the stream is used.

   @param position Pointer to a valid string pointer for use as stream state.
   @return An opened input stream, or all zeros on error.
*/
SERD_CONST_API SerdInputStream
serd_open_input_string(const char* ZIX_NONNULL* ZIX_NONNULL position);

/**
   Open a stream that reads from a file.

   This will open the file optimized for streaming I/O if possible.  To set
   things up differently, an arbitrary stream can be wrapped with
   serd_open_input_stream().

   @param path Path of file to open and read from.
   @return An opened input stream, or all zeros on error.
*/
SERD_API SerdInputStream
serd_open_input_file(const char* ZIX_NONNULL path);

/**
   Open a stream that reads from stdin.

   @return An opened input stream, or all zeros on error.
*/
SERD_PURE_API SerdInputStream
serd_open_input_standard(void);

/**
   Close an input stream.

   If the stream has a close function, it is called first, then the `stream`
   field is reset to null (so this is safe to call repeatedly on the same
   stream, provided the `close` function gracefully handles null).

   @return The status returned by the `close` function.
*/
SERD_API SerdStatus
serd_close_input(SerdInputStream* ZIX_NONNULL input);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_INPUT_STREAM_H
