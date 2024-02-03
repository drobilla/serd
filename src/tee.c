// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/tee.h"

#include "sink.h"

#include "serd/event.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "zix/allocator.h"

#include <assert.h>

typedef struct {
  struct SerdSinkImpl sink;
  const SerdSink*     first;
  const SerdSink*     second;
} SerdTee;

static SerdStatus
serd_tee_on_event(void* const handle, const SerdEvent* const event)
{
  SerdTee* const tee = (SerdTee*)handle;
  SerdStatus     st  = serd_sink_write_event(tee->first, event);

  return st ? st : serd_sink_write_event(tee->second, event);
}

SerdSink*
serd_tee_new(ZixAllocator* const   allocator,
             const SerdSink* const first,
             const SerdSink* const second)
{
  assert(first);
  assert(second);

  SerdTee* const tee = (SerdTee*)zix_calloc(allocator, 1, sizeof(SerdTee));

  if (tee) {
    tee->sink.allocator = allocator;
    tee->sink.handle    = tee;
    tee->sink.on_event  = serd_tee_on_event;
    tee->first          = first;
    tee->second         = second;
  }

  return (SerdSink*)tee;
}
