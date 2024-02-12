// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_INPUT_STREAM_H
#define SERD_INPUT_STREAM_H

#include "serd/attributes.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "zix/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_input_stream Input Streams
   @ingroup serd_reading_writing

   An input stream is used for reading input as a raw stream of bytes.  It is
   compatible with standard C `FILE` streams, but allows different functions to
   be provided for things like reading from a buffer or a socket.

   @{
*/

/// An input stream that produces bytes
typedef struct {
  void* ZIX_UNSPECIFIED      stream; ///< Opaque parameter for functions
  SerdReadFunc ZIX_NONNULL   read;   ///< Read bytes from input
  SerdCloseFunc ZIX_NULLABLE close;  ///< Close input
} SerdInputStream;

/**
   Open a stream that reads from a provided function.

   @param read_func Function to read input.
   @param close_func Function to close the stream after reading is done.
   @param stream Opaque stream parameter for functions.

   @return An opened input stream, or all zeros on error.
*/
SERD_CONST_API SerdInputStream
serd_open_input_stream(SerdReadFunc ZIX_NONNULL   read_func,
                       SerdCloseFunc ZIX_NULLABLE close_func,
                       void* ZIX_UNSPECIFIED      stream);

/**
   Open a stream that reads from a string.

   The string pointer that position points to must remain valid until the
   stream is closed.  This pointer serves as the internal stream state and will
   be mutated as the stream is used.

   @param position Pointer to a valid string pointer for use as stream state.
   @return An opened input stream, or all zeros on error.
*/
SERD_CONST_API SerdInputStream
serd_open_input_string(const char* ZIX_NONNULL* ZIX_NONNULL position);

/**
   Open a stream that reads from a file.

   An arbitrary `FILE*` can be used with serd_open_input_stream() as well, this
   convenience function opens the file properly for reading with serd, and sets
   flags for optimized I/O if possible.

   @param path Path of file to open and read from.
*/
SERD_API SerdInputStream
serd_open_input_file(const char* ZIX_NONNULL path);

/**
   Open a stream that reads from stdin.

   @return An opened input stream, or all zeros on error.
*/
SERD_API SerdInputStream
serd_open_input_standard(void);

/**
   Close an input stream.

   This will call the close function, and reset the stream internally so that
   no further reads can be made.  For convenience, this is safe to call on
   NULL, and safe to call several times on the same input.
*/
SERD_API SerdStatus
serd_close_input(SerdInputStream* ZIX_NULLABLE input);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_INPUT_STREAM_H
