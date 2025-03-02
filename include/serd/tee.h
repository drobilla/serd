// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TEE_H
#define SERD_TEE_H

#include <serd/attributes.h>
#include <serd/handler.h>
#include <serd/sink.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_tee Tee
   @ingroup serd_streaming
   @{
*/

/**
   Create a tee for sending an event stream to two sinks.

   Events are always sent in order, first to the "first" sink, then to the
   "second".

   @param allocator Allocator to use for the returned sink.

   @param first The sink to send events to first.

   @param second The sink to send events to second.

   @return A newly allocated struct which must be freed with
   serd_handler_free(), or null on errors.
*/
SERD_API SerdHandler* ZIX_ALLOCATED
serd_tee_new(ZixAllocator* ZIX_NULLABLE  allocator,
             const SerdSink* ZIX_NONNULL first,
             const SerdSink* ZIX_NONNULL second);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_TEE_H
