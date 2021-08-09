// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BYTE_SINK_H
#define SERD_BYTE_SINK_H

#include "serd/attributes.h"
#include "serd/buffer.h"
#include "serd/status.h"
#include "serd/stream.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_byte_sink Byte Sink
   @{
*/

/**
   A sink for bytes that receives output.

   Output from serd is UTF-8 encoded text.
*/
typedef struct SerdByteSinkImpl SerdByteSink;

/**
   Create a new byte sink that writes to a buffer.

   The `buffer` is owned by the caller, but will be expanded as necessary.
   Note that the string in the buffer will not be null terminated until the
   byte sink is closed.

   @param buffer Buffer to write output to.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new_buffer(SerdBuffer* SERD_NONNULL buffer);

/**
   Create a new byte sink that writes to a file.

   An arbitrary `FILE*` can be used via serd_byte_sink_new_function() as well,
   this is just a convenience function that opens the file properly and sets
   flags for optimized I/O if possible.

   @param path Path of file to open and write to.
   @param block_size Number of bytes to write per call.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new_filename(const char* SERD_NONNULL path, size_t block_size);

/**
   Create a new byte sink that writes to a user-specified function.

   The `stream` will be passed to the `write_func`, which is compatible with
   the standard C `fwrite` if `stream` is a `FILE*`.

   @param write_func Function called with bytes to consume.
   @param stream Context parameter passed to `sink`.
   @param block_size Number of bytes to write per call.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new_function(SerdWriteFunc SERD_NONNULL        write_func,
                            SerdStreamCloseFunc SERD_NULLABLE close_func,
                            void* SERD_NULLABLE               stream,
                            size_t                            block_size);

/// Flush any pending output in `sink` to the underlying write function
SERD_API
void
serd_byte_sink_flush(SerdByteSink* SERD_NONNULL sink);

/**
   Close `sink`, including the underlying file if necessary.

   If `sink` was created with serd_byte_sink_new_filename(), then the file is
   closed.  If there was an error, then SERD_ERR_UNKNOWN is returned and
   `errno` is set.
*/
SERD_API
SerdStatus
serd_byte_sink_close(SerdByteSink* SERD_NONNULL sink);

/// Free `sink`, flushing and closing first if necessary
SERD_API
void
serd_byte_sink_free(SerdByteSink* SERD_NULLABLE sink);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BYTE_SINK_H
