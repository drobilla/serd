// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_READER_H
#define SERD_READER_H

#include "serd/attributes.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "zix/attributes.h"

#include <stddef.h>
#include <stdint.h>

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
  /**
     Tolerate invalid input where possible.

     This will attempt to ignore invalid input and continue reading.  Invalid
     Unicode characters will be replaced with the replacement character, and
     various other syntactic problems will be ignored.  If there are more
     severe problems, the reader will try to skip the statement and continue
     parsing.  This should work reasonably well for line-based syntaxes like
     NTriples and NQuads, but abbreviated Turtle or TriG may not recover.

     Note that this flag should be used carefully, since it can result in data
     loss.
  */
  SERD_READ_LAX = 1U << 0U,

  /**
     Read generated blank node labels exactly without adjusting them.

     Normally, the reader will adapt blank node labels in the input that clash
     with its scheme for generating new ones, for example mapping "_:b123" to
     "_:B123".  This flag disables that, so that blank node labels are passed
     to the sink exactly as they are in the input.

     Note that this flag should be used carefully, since it can result in data
     corruption.  Specifically, if the input is a syntax like Turtle with
     anonymous nodes, the generated IDs for those nodes may clash with IDs from
     the input document.
  */
  SERD_READ_GENERATED = 1U << 1U,
} SerdReaderFlag;

/// Bitwise OR of SerdReaderFlag values
typedef uint32_t SerdReaderFlags;

/// Create a new RDF reader
SERD_API SerdReader* ZIX_ALLOCATED
serd_reader_new(SerdWorld* ZIX_NONNULL      world,
                SerdSyntax                  syntax,
                SerdReaderFlags             flags,
                const SerdSink* ZIX_NONNULL sink);

/**
   Set a prefix to be added to all blank node identifiers.

   This is useful when multiple files are to be parsed into the same output (a
   model or a file).  Since Serd preserves blank node IDs, this could cause
   conflicts where two non-equivalent blank nodes are merged, resulting in
   corrupt data.  By setting a unique blank node prefix for each parsed file,
   this can be avoided, while preserving blank node names.
*/
SERD_API void
serd_reader_add_blank_prefix(SerdReader* ZIX_NONNULL  reader,
                             const char* ZIX_NULLABLE prefix);

/**
   Prepare to read some input.

   This sets up the reader to read from the given input, but will not read any
   bytes from it.  This should be followed by serd_reader_read_chunk() or
   serd_reader_read_document() to actually read the input.

   @param reader The reader.
   @param input An opened input stream to read from.
   @param input_name The name of the input stream for error messages.
   @param block_size The number of bytes to read from the stream at once.
*/
SERD_API SerdStatus
serd_reader_start(SerdReader* ZIX_NONNULL      reader,
                  SerdInputStream* ZIX_NONNULL input,
                  const SerdNode* ZIX_NULLABLE input_name,
                  size_t                       block_size);

/**
   Read a single "chunk" of data during an incremental read.

   This function will read a single top level description, and return.  This
   may be a directive, statement, or several statements; essentially it reads
   until a '.' is encountered.  This is particularly useful for reading
   directly from a pipe or socket.
*/
SERD_API SerdStatus
serd_reader_read_chunk(SerdReader* ZIX_NONNULL reader);

/**
   Read a complete document from the source.

   This function will continue pulling from the source until a complete
   document has been read.  Note that this may block when used with streams,
   for incremental reading use serd_reader_read_chunk().
*/
SERD_API SerdStatus
serd_reader_read_document(SerdReader* ZIX_NONNULL reader);

/**
   Finish reading from the source.

   This should be called before starting to read from another source.
*/
SERD_API SerdStatus
serd_reader_finish(SerdReader* ZIX_NONNULL reader);

/**
   Skip over bytes in the input until a specific byte is encountered.

   Typically used for recording from errors in a line-based syntax by skipping
   ahead to the next newline.

   @return #SERD_SUCCESS if the given byte was reached, or #SERD_FAILURE if the
   end of input is reached.
*/
SERD_API SerdStatus
serd_reader_skip_until_byte(SerdReader* ZIX_NONNULL reader, uint8_t byte);

/**
   Free `reader`.

   The reader will be finished via `serd_reader_finish()` if necessary.
*/
SERD_API void
serd_reader_free(SerdReader* ZIX_NULLABLE reader);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_READER_H
