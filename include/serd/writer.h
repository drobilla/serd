// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_WRITER_H
#define SERD_WRITER_H

#include "serd/attributes.h"
#include "serd/env.h"
#include "serd/output_stream.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stddef.h>
#include <stdint.h>

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
  /**
     Escape additional characters as in RDF Test Cases format.

     This writes "extended" characters as printable ASCII, using "U" escapes in
     URIs and strings where necessary.  This is the format used by the outputs
     in the Turtle test suite (which predates RDF 1.1 NTriples).  This style
     makes NTriples output non-canonical, so it generally shouldn't be used
     except for compatibility purposes.  See
     <https://www.w3.org/TR/rdf-testcases/>.
  */
  SERD_WRITE_ESCAPED = 1U << 0U,

  /**
     Write expanded URIs instead of prefixed names.

     This will avoid shortening URIs into CURIEs entirely, even if the output
     syntax supports prefixed names.  This can be useful for making chunks of
     syntax context-free.
  */
  SERD_WRITE_EXPANDED = 1U << 1U,

  /**
     Write URI references exactly as they are received.

     Normally, the writer resolves URIs against the base URI, so it can
     potentially write them as relative URI references.  This flag disables
     that, so URI nodes are written exactly as they are received.
  */
  SERD_WRITE_VERBATIM = 1U << 2U,

  /**
     Write terser output without newlines.

     For Turtle and TriG, this enables a terser form of output which only has
     newlines at the top level.  This can result in very long lines, but is
     more compact and useful for making these abbreviated syntaxes line-based.
  */
  SERD_WRITE_TERSE = 1U << 3U,

  /**
     Tolerate lossy output.

     This will tolerate input that can not be written without loss, in
     particular invalid UTF-8 text.  Note that this flag should be used
     carefully, since it can result in data loss.
  */
  SERD_WRITE_LAX = 1U << 4U,

  /**
     Suppress writing directives that describe the context.

     This writes data as usual, but suppresses writing `prefix` directives in
     Turtle and TriG.  The resulting output is a fragment of a document with
     implicit context, so it will only be readable in a suitable enviromnent.
  */
  SERD_WRITE_CONTEXTUAL = 1U << 5U,

  /**
     Write rdf:type as a normal predicate.

     This disables the special "a" syntax in Turtle and TriG.
  */
  SERD_WRITE_LONGHAND = 1U << 6U,
} SerdWriterFlag;

/// Bitwise OR of #SerdWriterFlag values
typedef uint32_t SerdWriterFlags;

/// Create a new RDF writer
SERD_API SerdWriter* ZIX_ALLOCATED
serd_writer_new(SerdWorld* ZIX_NONNULL        world,
                SerdSyntax                    syntax,
                SerdWriterFlags               flags,
                const SerdEnv* ZIX_NONNULL    env,
                SerdOutputStream* ZIX_NONNULL output,
                size_t                        block_size);

/// Free `writer`
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
