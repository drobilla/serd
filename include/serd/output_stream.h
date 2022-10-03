// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_OUTPUT_STREAM_H
#define SERD_OUTPUT_STREAM_H

#include "serd/attributes.h"
#include "serd/buffer.h"
#include "serd/status.h"
#include "serd/stream.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_output_stream Output Streams
   @ingroup serd_reading_writing
   @{
*/

/**
   An output stream that receives bytes.

   An output stream is used for writing output as a raw stream of bytes.  It is
   compatible with standard C `FILE` streams, but allows different functions to
   be provided for things like writing to a buffer or a socket.

   Output from serd is UTF-8 encoded text.
*/
typedef struct {
  void* SERD_NULLABLE         stream; ///< Opaque parameter for functions
  SerdWriteFunc SERD_NONNULL  write;  ///< Write bytes to output
  SerdErrorFunc SERD_NULLABLE error;  ///< Stream error accessor
  SerdCloseFunc SERD_NULLABLE close;  ///< Close output
} SerdOutputStream;

/**
   Open a stream that writes to a provided function.

   @param write_func Function to write bytes to the stream.
   @param error_func Function to detect errors in the stream.
   @param close_func Function to close the stream.
   @param stream Stream parameter passed to `write_func` and `close_func`.
   @return An opened output stream, or all zeros on error.
*/
SERD_CONST_API
SerdOutputStream
serd_open_output_stream(SerdWriteFunc SERD_NONNULL  write_func,
                        SerdErrorFunc SERD_NULLABLE error_func,
                        SerdCloseFunc SERD_NULLABLE close_func,
                        void* SERD_NULLABLE         stream);

/**
   Open a stream that writes to a buffer.

   The `buffer` is owned by the caller, but will be expanded using `realloc` as
   necessary.  Note that the string in the buffer will not be null terminated
   until the stream is closed.

   @param buffer Buffer to write output to.
   @return An opened output stream, or all zeros on error.
*/
SERD_CONST_API
SerdOutputStream
serd_open_output_buffer(SerdBuffer* SERD_NONNULL buffer);

/**
   Open a stream that writes to a file.

   An arbitrary `FILE*` can be used with serd_open_output_stream() as well,
   this convenience function opens the file properly for writing with serd, and
   sets flags for optimized I/O if possible.

   @param path Path of file to open and write to.
*/
SERD_API
SerdOutputStream
serd_open_output_file(const char* SERD_NONNULL path);

/**
   Close an output stream.

   This will call the close function, and reset the stream internally so that
   no further writes can be made.  For convenience, this is safe to call on
   NULL, and safe to call several times on the same output.  Failure is
   returned in both of those cases.
*/
SERD_API
SerdStatus
serd_close_output(SerdOutputStream* SERD_NULLABLE output);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_OUTPUT_STREAM_H
