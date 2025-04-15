// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_WRITER_H
#define SERD_WRITER_H

#include <serd/attributes.h>
#include <serd/env.h>
#include <serd/output_stream.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/world.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_writer Writer
   @ingroup serd_reading_writing
   @{
*/

/// Streaming serialiser that writes a text stream as statements are pushed
typedef struct SerdWriterImpl SerdWriter;

/**
   Writer style options.

   These flags allow more precise control of writer output style.  Note that
   some options are only supported for some syntaxes, for example, NTriples
   does not support abbreviation and is always ASCII.
*/
typedef enum {
  SERD_WRITE_ASCII       = 1U << 0U, ///< Escape all non-ASCII characters
  SERD_WRITE_UNQUALIFIED = 1U << 1U, ///< Don't shorten URIs into CURIEs
  SERD_WRITE_UNRESOLVED  = 1U << 2U, ///< Don't make URIs relative
  SERD_WRITE_LAX         = 1U << 3U, ///< Tolerate lossy output
  SERD_WRITE_TERSE       = 1U << 4U, ///< Write terse output without newlines
} SerdWriterFlag;

/// Bitwise OR of #SerdWriterFlag values
typedef unsigned SerdWriterFlags;

/// Create a new RDF writer
SERD_API SerdWriter* ZIX_ALLOCATED
serd_writer_new(SerdWorld* ZIX_NONNULL world,
                SerdSyntax             syntax,
                SerdWriterFlags        flags,
                SerdEnv* ZIX_NONNULL   env);

/**
   Free `writer`.

   The writer will be finished via `serd_writer_finish()` if necessary.
*/
SERD_API void
serd_writer_free(SerdWriter* ZIX_NULLABLE writer);

/// Return a sink interface that emits statements via `writer`
SERD_CONST_API const SerdSink* ZIX_NONNULL
serd_writer_sink(SerdWriter* ZIX_NONNULL writer);

/**
   Set the current root URI.

   The root URI should be a prefix of the base URI.  The path of the root URI
   is the highest path any relative up-reference can refer to.  For example,
   with root <file:///foo/root> and base <file:///foo/root/base>,
   <file:///foo/root> will be written as <../>, but <file:///foo> will be
   written non-relatively as <file:///foo>.  If the root is not explicitly set,
   it defaults to the base URI, so no up-references will be created at all.
*/
SERD_API SerdStatus
serd_writer_set_root_uri(SerdWriter* ZIX_NONNULL writer, ZixStringView uri);

/**
   Prepare to write some output.

   This sets up the writer to write to the given output.  This should be
   followed by sending events to the writer's sink via serd_writer_sink() to
   actually write the output.

   @param writer The writer.
   @param output An opened output stream to write to.
   @param block_size The number of bytes to write to the stream at once.
*/
SERD_API SerdStatus
serd_writer_start(SerdWriter* ZIX_NONNULL             writer,
                  const SerdOutputStream* ZIX_NONNULL output,
                  size_t                              block_size);

/**
   Finish a write.

   This flushes any pending output, for example terminating punctuation, so
   that the output is a complete document.
*/
SERD_API SerdStatus
serd_writer_finish(SerdWriter* ZIX_NONNULL writer);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_WRITER_H
