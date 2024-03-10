// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SINK_H
#define SERD_SINK_H

#include "serd/attributes.h"
#include "serd/caret_view.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_sink Sink
   @ingroup serd_streaming
   @{
*/

/**
   Sink function for base URI changes.

   Called whenever the base URI of the serialisation changes.
*/
typedef SerdStatus (*SerdBaseFunc)(void* ZIX_UNSPECIFIED       handle,
                                   const SerdNode* ZIX_NONNULL uri);

/**
   Sink function for namespace definitions.

   Called whenever a prefix is defined in the serialisation.
*/
typedef SerdStatus (*SerdPrefixFunc)(void* ZIX_UNSPECIFIED       handle,
                                     const SerdNode* ZIX_NONNULL name,
                                     const SerdNode* ZIX_NONNULL uri);

/**
   Sink function for statements.

   Called for every RDF statement in the serialisation.
*/
typedef SerdStatus (*SerdStatementFunc)(void* ZIX_UNSPECIFIED   handle,
                                        SerdStatementEventFlags flags,
                                        SerdStatementView       statement);

/**
   Sink function for anonymous node end markers.

   This is called to indicate that the anonymous node with the given `value`
   will no longer be referred to by any future statements (so the anonymous
   node is finished).
*/
typedef SerdStatus (*SerdEndFunc)(void* ZIX_UNSPECIFIED       handle,
                                  const SerdNode* ZIX_NONNULL node);

/// An interface that receives a stream of RDF data
typedef struct SerdSinkImpl SerdSink;

/// Function to free an opaque handle
typedef void (*SerdFreeFunc)(void* ZIX_NULLABLE ptr);

/**
   Create a new sink.

   @param allocator Allocator to use for the returned sink.
   @param handle Opaque handle that will be passed to sink functions.
   @param event_func Function that will be called for every event.
   @param free_handle Free function to call on handle in serd_sink_free().
*/
SERD_API SerdSink* ZIX_ALLOCATED
serd_sink_new(ZixAllocator* ZIX_NULLABLE allocator,
              void* ZIX_UNSPECIFIED      handle,
              SerdEventFunc ZIX_NULLABLE event_func,
              SerdFreeFunc ZIX_NULLABLE  free_handle);

/// Free `sink`
SERD_API void
serd_sink_free(SerdSink* ZIX_NULLABLE sink);

/// Send an event to the sink
SERD_API SerdStatus
serd_sink_write_event(const SerdSink* ZIX_NONNULL  sink,
                      const SerdEvent* ZIX_NONNULL event);

/// Set the base URI
SERD_API SerdStatus
serd_sink_write_base(const SerdSink* ZIX_NONNULL sink,
                     const SerdNode* ZIX_NONNULL uri);

/// Set a namespace prefix
SERD_API SerdStatus
serd_sink_write_prefix(const SerdSink* ZIX_NONNULL sink,
                       const SerdNode* ZIX_NONNULL name,
                       const SerdNode* ZIX_NONNULL uri);

/// Write a statement
SERD_API SerdStatus
serd_sink_write_statement(const SerdSink* ZIX_NONNULL sink,
                          SerdStatementEventFlags     flags,
                          SerdStatementView           statement);

/// Write a statement with a caret
SERD_API SerdStatus
serd_sink_write_statement_from(const SerdSink* ZIX_NONNULL sink,
                               SerdStatementEventFlags     flags,
                               SerdStatementView           statement,
                               SerdCaretView               caret);

/// Write a statement from individual nodes
SERD_API SerdStatus
serd_sink_write(const SerdSink* ZIX_NONNULL  sink,
                SerdStatementEventFlags      flags,
                const SerdNode* ZIX_NONNULL  subject,
                const SerdNode* ZIX_NONNULL  predicate,
                const SerdNode* ZIX_NONNULL  object,
                const SerdNode* ZIX_NULLABLE graph);

/// Mark the end of an anonymous node
SERD_API SerdStatus
serd_sink_write_end(const SerdSink* ZIX_NONNULL sink,
                    const SerdNode* ZIX_NONNULL node);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_SINK_H
