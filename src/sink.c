// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/event.h>
#include <serd/sink.h>
#include <serd/status.h>

#include <assert.h>

SerdStatus
serd_sink_event(const SerdSink* sink, const SerdEvent event)
{
  assert(sink);
  return (!event.type || event.type > SERD_EVENT_END) ? SERD_BAD_ARG
         : sink->on_event ? sink->on_event(sink->handle, &event)
                          : SERD_SUCCESS;
}
