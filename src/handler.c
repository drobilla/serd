// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/handler.h>
#include <serd/sink.h>
#include <zix/allocator.h>

#include <assert.h>
#include <stddef.h>

struct SerdHandlerImpl {
  SerdSink        sink;
  ZixAllocator*   allocator;
  SerdDestroyFunc destroy_data;
  size_t          data_size;
};

SerdHandler*
serd_handler_new(ZixAllocator* const   allocator,
                 const SerdSinkFunc    on_event,
                 const SerdDestroyFunc destroy_data,
                 const size_t          data_size)
{
  assert(on_event);

  SerdHandler* const handler =
    (SerdHandler*)zix_calloc(allocator, 1, sizeof(SerdHandler) + data_size);

  if (handler) {
    handler->sink.handle   = data_size ? (handler + 1U) : NULL;
    handler->sink.on_event = on_event;
    handler->allocator     = allocator;
    handler->destroy_data  = destroy_data;
    handler->data_size     = data_size;
  }

  return handler;
}

void
serd_handler_free(SerdHandler* const handler)
{
  if (handler) {
    if (handler->destroy_data) {
      handler->destroy_data(serd_handler_data(handler));
    }

    zix_free(handler->allocator, handler);
  }
}

void*
serd_handler_data(SerdHandler* const handler)
{
  assert(handler);
  return (handler->data_size) ? (handler + 1U) : NULL;
}

const SerdSink*
serd_handler_sink(const SerdHandler* const handler)
{
  assert(handler);
  return &handler->sink;
}
