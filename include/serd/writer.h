// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_WRITER_H
#define SERD_WRITER_H

#include "serd/attributes.h"
#include "serd/env.h"
#include "serd/error.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/syntax.h"
#include "serd/uri.h"

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
  SERD_WRITE_ASCII       = 1U << 0U, ///< Escape all non-ASCII characters
  SERD_WRITE_UNQUALIFIED = 1U << 1U, ///< Do not shorten URIs into CURIEs
  SERD_WRITE_UNRESOLVED  = 1U << 2U, ///< Do not make URIs relative
  SERD_WRITE_BULK        = 1U << 3U, ///< Write output in pages
  SERD_WRITE_STRICT      = 1U << 4U, ///< Abort with error on lossy output
  SERD_WRITE_TERSE       = 1U << 5U, ///< Write terser output without newlines
} SerdWriterFlag;

/// Bitwise OR of #SerdWriterFlag values
typedef uint32_t SerdWriterFlags;

/// Create a new RDF writer
SERD_API SerdWriter* SERD_ALLOCATED
serd_writer_new(SerdSyntax                       syntax,
                SerdWriterFlags                  flags,
                SerdEnv* SERD_NONNULL            env,
                const SerdURIView* SERD_NULLABLE base_uri,
                SerdSink SERD_NONNULL            ssink,
                void* SERD_UNSPECIFIED           stream);

/// Free `writer`
SERD_API void
serd_writer_free(SerdWriter* SERD_NULLABLE writer);

/// Return the env used by `writer`
SERD_PURE_API
SerdEnv* SERD_NONNULL
serd_writer_env(SerdWriter* SERD_NONNULL writer);

/**
   A convenience sink function for writing to a FILE*.

   This function can be used as a #SerdSink when writing to a FILE*.  The
   `stream` parameter must be a FILE* opened for writing.
*/
SERD_API size_t
serd_file_sink(const void* SERD_NONNULL buf,
               size_t                   len,
               void* SERD_UNSPECIFIED   stream);

/**
   Set a function to be called when errors occur during writing.

   The `error_func` will be called with `handle` as its first argument.  If
   no error function is set, errors are printed to stderr.
*/
SERD_API void
serd_writer_set_error_sink(SerdWriter* SERD_NONNULL   writer,
                           SerdErrorFunc SERD_NONNULL error_func,
                           void* SERD_UNSPECIFIED     error_handle);

/**
   Set a prefix to be removed from matching blank node identifiers.

   This is the counterpart to serd_reader_add_blank_prefix() which can be used
   to "undo" added prefixes.
*/
SERD_API void
serd_writer_chop_blank_prefix(SerdWriter* SERD_NONNULL  writer,
                              const char* SERD_NULLABLE prefix);

/**
   Set the current output base URI, and emit a directive if applicable.

   Note this function can be safely casted to #SerdBaseFunc.
*/
SERD_API SerdStatus
serd_writer_set_base_uri(SerdWriter* SERD_NONNULL      writer,
                         const SerdNode* SERD_NULLABLE uri);

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
serd_writer_set_root_uri(SerdWriter* SERD_NONNULL      writer,
                         const SerdNode* SERD_NULLABLE uri);

/**
   Set a namespace prefix (and emit directive if applicable).

   Note this function can be safely casted to #SerdPrefixFunc.
*/
SERD_API SerdStatus
serd_writer_set_prefix(SerdWriter* SERD_NONNULL     writer,
                       const SerdNode* SERD_NONNULL name,
                       const SerdNode* SERD_NONNULL uri);

/**
   Write a statement.

   Note this function can be safely casted to #SerdStatementFunc.
*/
SERD_API SerdStatus
serd_writer_write_statement(SerdWriter* SERD_NONNULL      writer,
                            SerdStatementFlags            flags,
                            const SerdNode* SERD_NULLABLE graph,
                            const SerdNode* SERD_NONNULL  subject,
                            const SerdNode* SERD_NONNULL  predicate,
                            const SerdNode* SERD_NONNULL  object,
                            const SerdNode* SERD_NULLABLE datatype,
                            const SerdNode* SERD_NULLABLE lang);

/**
   Mark the end of an anonymous node's description.

   Note this function can be safely casted to #SerdEndFunc.
*/
SERD_API SerdStatus
serd_writer_end_anon(SerdWriter* SERD_NONNULL      writer,
                     const SerdNode* SERD_NULLABLE node);

/**
   Finish a write.

   This flushes any pending output, for example terminating punctuation, so
   that the output is a complete document.
*/
SERD_API SerdStatus
serd_writer_finish(SerdWriter* SERD_NONNULL writer);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_WRITER_H
