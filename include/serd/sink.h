// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SINK_H
#define SERD_SINK_H

#include <serd/attributes.h>
#include <serd/event.h>
#include <serd/status.h>
#include <zix/attributes.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_sink Sink
   @ingroup serd_streaming
   @{
*/

/// Function for handling events
typedef SerdStatus (*SerdSinkFunc)(void* ZIX_UNSPECIFIED        handle,
                                   const SerdEvent* ZIX_NONNULL event);

/// An interface that receives a stream of data events
typedef struct {
  void* ZIX_UNSPECIFIED        handle;   ///< Opaque handle
  SerdSinkFunc ZIX_UNSPECIFIED on_event; ///< Event handling function
} SerdSink;

/**
   Send an event to `sink`.

   This is just a convenience for calling the sink's function with an event by
   value.
*/
SERD_API SerdStatus
serd_sink_event(const SerdSink* ZIX_NONNULL sink, SerdEvent event);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_SINK_H
