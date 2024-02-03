// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/tee.h"

#include "serd/event.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stddef.h>

typedef struct {
  ZixAllocator*   allocator;
  const SerdSink* first;
  const SerdSink* second;
} SerdTeeData;

static SerdStatus
serd_tee_on_event(void* const handle, const SerdEvent* const event)
{
  SerdTeeData* const data = (SerdTeeData*)handle;
  SerdStatus         st   = serd_sink_write_event(data->first, event);

  return st ? st : serd_sink_write_event(data->second, event);
}

static SerdTeeData*
serd_tee_data_new(ZixAllocator* const   allocator,
                  const SerdSink* const first,
                  const SerdSink* const second)
{
  SerdTeeData* const data =
    (SerdTeeData*)zix_malloc(allocator, sizeof(SerdTeeData));

  if (data) {
    data->allocator = allocator;
    data->first     = first;
    data->second    = second;
  }

  return data;
}

static void
serd_tee_data_free(void* const ptr)
{
  SerdTeeData* const data = (SerdTeeData*)ptr;
  zix_free(data->allocator, data);
}

SerdSink*
serd_tee_new(ZixAllocator* const   allocator,
             const SerdSink* const first,
             const SerdSink* const second)
{
  assert(first);
  assert(second);

  SerdTeeData* const data = serd_tee_data_new(allocator, first, second);

  return data ? serd_sink_new(allocator,
                              data,
                              serd_tee_on_event,
                              (SerdFreeFunc)serd_tee_data_free)
              : NULL;
}
