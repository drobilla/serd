// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SINK_H
#define SERD_SINK_H

#include "serd/attributes.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/world.h"

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
typedef SerdStatus (*SerdBaseFunc)(void* SERD_NULLABLE          handle,
                                   const SerdNode* SERD_NONNULL uri);

/**
   Sink function for namespace definitions.

   Called whenever a prefix is defined in the serialisation.
*/
typedef SerdStatus (*SerdPrefixFunc)(void* SERD_NULLABLE          handle,
                                     const SerdNode* SERD_NONNULL name,
                                     const SerdNode* SERD_NONNULL uri);

/**
   Sink function for statements.

   Called for every RDF statement in the serialisation.
*/
typedef SerdStatus (*SerdStatementFunc)(void* SERD_NULLABLE handle,
                                        SerdStatementFlags  flags,
                                        const SerdStatement* SERD_NONNULL
                                          statement);

/**
   Sink function for anonymous node end markers.

   This is called to indicate that the anonymous node with the given `value`
   will no longer be referred to by any future statements (so the anonymous
   node is finished).
*/
typedef SerdStatus (*SerdEndFunc)(void* SERD_NULLABLE          handle,
                                  const SerdNode* SERD_NONNULL node);

/// An interface that receives a stream of RDF data
typedef struct SerdSinkImpl SerdSink;

/// Function to free an opaque handle
typedef void (*SerdFreeFunc)(void* SERD_NULLABLE ptr);

/**
   Create a new sink.

   @param world The world to create the sink in.
   @param handle Opaque handle that will be passed to sink functions.
   @param event_func Function that will be called for every event.
   @param free_handle Free function to call on handle in serd_sink_free().
*/
SERD_API
SerdSink* SERD_ALLOCATED
serd_sink_new(const SerdWorld* SERD_NONNULL world,
              void* SERD_NULLABLE           handle,
              SerdEventFunc SERD_NULLABLE   event_func,
              SerdFreeFunc SERD_NULLABLE    free_handle);

/// Free `sink`
SERD_API
void
serd_sink_free(SerdSink* SERD_NULLABLE sink);

/// Send an event to the sink
SERD_API
SerdStatus
serd_sink_write_event(const SerdSink* SERD_NONNULL  sink,
                      const SerdEvent* SERD_NONNULL event);

/// Set the base URI
SERD_API
SerdStatus
serd_sink_write_base(const SerdSink* SERD_NONNULL sink,
                     const SerdNode* SERD_NONNULL uri);

/// Set a namespace prefix
SERD_API
SerdStatus
serd_sink_write_prefix(const SerdSink* SERD_NONNULL sink,
                       const SerdNode* SERD_NONNULL name,
                       const SerdNode* SERD_NONNULL uri);

/// Write a statement
SERD_API
SerdStatus
serd_sink_write_statement(const SerdSink* SERD_NONNULL      sink,
                          SerdStatementFlags                flags,
                          const SerdStatement* SERD_NONNULL statement);

/// Write a statement from individual nodes
SERD_API
SerdStatus
serd_sink_write(const SerdSink* SERD_NONNULL  sink,
                SerdStatementFlags            flags,
                const SerdNode* SERD_NONNULL  subject,
                const SerdNode* SERD_NONNULL  predicate,
                const SerdNode* SERD_NONNULL  object,
                const SerdNode* SERD_NULLABLE graph);

/// Mark the end of an anonymous node
SERD_API
SerdStatus
serd_sink_write_end(const SerdSink* SERD_NONNULL sink,
                    const SerdNode* SERD_NONNULL node);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_SINK_H
