// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BYTE_SOURCE_H
#define SERD_BYTE_SOURCE_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/stream.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_byte_source Byte Source
   @ingroup serd
   @{
*/

/// A source for bytes that provides text input
typedef struct SerdByteSourceImpl SerdByteSource;

/**
   Create a new byte source that reads from a string.

   @param string Null-terminated UTF-8 string to read from.
   @param name Optional name of stream for error messages (string or URI).
*/
SERD_API
SerdByteSource* SERD_ALLOCATED
serd_byte_source_new_string(const char* SERD_NONNULL      string,
                            const SerdNode* SERD_NULLABLE name);

/**
   Create a new byte source that reads from a file.

   An arbitrary `FILE*` can be used via serd_byte_source_new_function() as
   well, this is just a convenience function that opens the file properly, sets
   flags for optimized I/O if possible, and automatically sets the name of the
   source to the file path.

   @param path Path of file to open and read from.
   @param page_size Number of bytes to read per call.
*/
SERD_API
SerdByteSource* SERD_ALLOCATED
serd_byte_source_new_filename(const char* SERD_NONNULL path, size_t page_size);

/**
   Create a new byte source that reads from a user-specified function

   The `stream` will be passed to the `read_func`, which is compatible with the
   standard C `fread` if `stream` is a `FILE*`.  Note that the reader only ever
   reads individual bytes at a time, that is, the `size` parameter will always
   be 1 (but `nmemb` may be higher).

   @param read_func Stream read function, like `fread`.
   @param error_func Stream error function, like `ferror`.
   @param close_func Stream close function, like `fclose`.
   @param stream Context parameter passed to `read_func` and `error_func`.
   @param name Optional name of stream for error messages (string or URI).
   @param page_size Number of bytes to read per call.
*/
SERD_API
SerdByteSource* SERD_ALLOCATED
serd_byte_source_new_function(SerdReadFunc SERD_NONNULL         read_func,
                              SerdStreamErrorFunc SERD_NONNULL  error_func,
                              SerdStreamCloseFunc SERD_NULLABLE close_func,
                              void* SERD_NULLABLE               stream,
                              const SerdNode* SERD_NULLABLE     name,
                              size_t                            page_size);

/// Free `source`
SERD_API
void
serd_byte_source_free(SerdByteSource* SERD_NULLABLE source);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BYTE_SOURCE_H
