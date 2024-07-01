// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sink.h"

#include "serd/event.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "zix/allocator.h"

#include <assert.h>

SerdSink*
serd_sink_new(ZixAllocator* const allocator,
              void* const         handle,
              SerdEventFunc       event_func,
              SerdFreeFunc        free_handle)
{
  SerdSink* sink = (SerdSink*)zix_calloc(allocator, 1, sizeof(SerdSink));

  if (sink) {
    sink->allocator   = allocator;
    sink->handle      = handle;
    sink->on_event    = event_func;
    sink->free_handle = free_handle;
  }

  return sink;
}

void
serd_sink_free(SerdSink* sink)
{
  if (sink) {
    if (sink->free_handle) {
      sink->free_handle(sink->handle);
    }

    zix_free(sink->allocator, sink);
  }
}

SerdStatus
serd_sink_write_event(const SerdSink* sink, const SerdEvent event)
{
  assert(sink);
  return sink->on_event ? sink->on_event(sink->handle, &event) : SERD_SUCCESS;
}
