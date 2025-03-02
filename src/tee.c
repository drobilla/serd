// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/tee.h>

#include <serd/event.h>
#include <serd/handler.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <zix/allocator.h>

#include <assert.h>
#include <stddef.h>

typedef struct {
  const SerdSink* first;
  const SerdSink* second;
} SerdTeeData;

static SerdStatus
serd_tee_on_event(void* const handle, const SerdEvent* const event)
{
  const SerdTeeData* const data = (const SerdTeeData*)handle;

  SerdStatus st = serd_sink_event(data->first, *event);
  if (!st) {
    st = serd_sink_event(data->second, *event);
  }

  return st;
}

SerdHandler*
serd_tee_new(ZixAllocator* const   allocator,
             const SerdSink* const first,
             const SerdSink* const second)
{
  assert(first);
  assert(second);

  SerdHandler* const tee =
    serd_handler_new(allocator, serd_tee_on_event, NULL, sizeof(SerdTeeData));

  if (tee) {
    SerdTeeData* const data = (SerdTeeData*)serd_handler_data(tee);

    data->first  = first;
    data->second = second;
  }

  return tee;
}
