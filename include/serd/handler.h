// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_HANDLER_H
#define SERD_HANDLER_H

#include <serd/attributes.h>
#include <serd/sink.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_handler Handler
   @ingroup serd_streaming
   @{
*/

/**
   A dynamically allocated event handler with a #SerdSink interface.

   This is a convenience facility to manage function objects that contain some
   opaque user data and provide (only) a #SerdSink interface.
*/
typedef struct SerdHandlerImpl SerdHandler;

/// Function to destroy opaque data before being freed
typedef void (*SerdDestroyFunc)(void* ZIX_NULLABLE ptr);

/**
   Create a new handler.

   @param allocator Allocator to use for the returned handler.
   @param event_func Function that will be called for every event.
   @param destroy_data Function to destroy additional data when freed.
   @param data_size Size of additional data in bytes.
*/
SERD_API SerdHandler* ZIX_ALLOCATED
serd_handler_new(ZixAllocator* ZIX_NULLABLE   allocator,
                 SerdSinkFunc ZIX_NONNULL     event_func,
                 SerdDestroyFunc ZIX_NULLABLE destroy_data,
                 size_t                       data_size);

/// Free `handler`
SERD_API void
serd_handler_free(SerdHandler* ZIX_NULLABLE handler);

/// Return a pointer to the opaque additional data of `handler`
SERD_CONST_API void* ZIX_UNSPECIFIED
serd_handler_data(SerdHandler* ZIX_NONNULL handler);

/// Return the sink interface of `handler`
SERD_CONST_API const SerdSink* ZIX_NONNULL
serd_handler_sink(const SerdHandler* ZIX_NONNULL handler);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_HANDLER_H
