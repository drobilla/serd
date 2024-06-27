// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_READER_H
#define SERD_READER_H

#include "serd/attributes.h"
#include "serd/error.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/syntax.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_reader Reader
   @ingroup serd_reading_writing
   @{
*/

/// Streaming parser that reads a text stream and writes to a statement sink
typedef struct SerdReaderImpl SerdReader;

/// Reader options
typedef enum {
  SERD_READ_LAX = 1U << 0U, ///< Tolerate invalid input where possible
} SerdReaderFlag;

/// Bitwise OR of SerdReaderFlag values
typedef uint32_t SerdReaderFlags;

/// Create a new RDF reader
SERD_API SerdReader* SERD_ALLOCATED
serd_reader_new(SerdSyntax             syntax,
                SerdReaderFlags        flags,
                void* SERD_UNSPECIFIED handle,
                void (*SERD_NULLABLE free_handle)(void* SERD_NULLABLE),
                SerdBaseFunc SERD_NULLABLE      base_func,
                SerdPrefixFunc SERD_NULLABLE    prefix_func,
                SerdStatementFunc SERD_NULLABLE statement_func,
                SerdEndFunc SERD_NULLABLE       end_func);

/**
   Set a function to be called when errors occur during reading.

   The `error_func` will be called with `handle` as its first argument.  If
   no error function is set, errors are printed to stderr in GCC style.
*/
SERD_API void
serd_reader_set_error_sink(SerdReader* SERD_NONNULL  reader,
                           SerdLogFunc SERD_NULLABLE error_func,
                           void* SERD_UNSPECIFIED    error_handle);

/// Return the `handle` passed to serd_reader_new()
SERD_PURE_API
void* SERD_UNSPECIFIED
serd_reader_handle(const SerdReader* SERD_NONNULL reader);

/**
   Set a prefix to be added to all blank node identifiers.

   This is useful when multiple files are to be parsed into the same output (a
   model or a file).  Since Serd preserves blank node IDs, this could cause
   conflicts where two non-equivalent blank nodes are merged, resulting in
   corrupt data.  By setting a unique blank node prefix for each parsed file,
   this can be avoided, while preserving blank node names.
*/
SERD_API void
serd_reader_add_blank_prefix(SerdReader* SERD_NONNULL  reader,
                             const char* SERD_NULLABLE prefix);

/// Read a file at a given `uri`
SERD_API SerdStatus
serd_reader_read_file(SerdReader* SERD_NONNULL reader,
                      const char* SERD_NONNULL uri);

/**
   Start an incremental read from a file handle.

   Iff `bulk` is true, `file` will be read a page at a time.  This is more
   efficient, but uses a page of memory and means that an entire page of input
   must be ready before any callbacks will fire.  To react as soon as input
   arrives, set `bulk` to false.
*/
SERD_API SerdStatus
serd_reader_start_stream(SerdReader* SERD_NONNULL  reader,
                         FILE* SERD_NONNULL        file,
                         const char* SERD_NULLABLE name,
                         bool                      bulk);

/**
   Start an incremental read from a user-specified source.

   The `read_func` is guaranteed to only be called for `page_size` elements
   with size 1 (i.e. `page_size` bytes).
*/
SERD_API SerdStatus
serd_reader_start_source_stream(SerdReader* SERD_NONNULL   reader,
                                SerdReadFunc SERD_NONNULL  read_func,
                                SerdErrorFunc SERD_NONNULL error_func,
                                void* SERD_UNSPECIFIED     stream,
                                const char* SERD_NULLABLE  name,
                                size_t                     page_size);

/**
   Read a single "chunk" of data during an incremental read.

   This function will read a single top level description, and return.  This
   may be a directive, statement, or several statements; essentially it reads
   until a '.' is encountered.  This is particularly useful for reading
   directly from a pipe or socket.
*/
SERD_API SerdStatus
serd_reader_read_chunk(SerdReader* SERD_NONNULL reader);

/// Finish an incremental read from a file handle
SERD_API SerdStatus
serd_reader_end_stream(SerdReader* SERD_NONNULL reader);

/// Read `file`
SERD_API SerdStatus
serd_reader_read_file_handle(SerdReader* SERD_NONNULL  reader,
                             FILE* SERD_NONNULL        file,
                             const char* SERD_NULLABLE name);

/// Read a user-specified byte source
SERD_API SerdStatus
serd_reader_read_source(SerdReader* SERD_NONNULL   reader,
                        SerdReadFunc SERD_NONNULL  source,
                        SerdErrorFunc SERD_NONNULL error,
                        void* SERD_UNSPECIFIED     stream,
                        const char* SERD_NULLABLE  name,
                        size_t                     page_size);

/// Read `utf8`
SERD_API SerdStatus
serd_reader_read_string(SerdReader* SERD_NONNULL reader,
                        const char* SERD_NONNULL utf8);

/**
   Skip over bytes in the input until a specific byte is encountered.

   Typically used for recording from errors in a line-based syntax by skipping
   ahead to the next newline.

   @return #SERD_SUCCESS if the given byte was reached, or #SERD_FAILURE if the
   end of input is reached.
*/
SERD_API SerdStatus
serd_reader_skip_until_byte(SerdReader* SERD_NONNULL reader, uint8_t byte);

/// Free `reader`
SERD_API void
serd_reader_free(SerdReader* SERD_NULLABLE reader);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_READER_H
