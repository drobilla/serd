// Copyright 2019-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/filter.h"

#include "serd/event.h"
#include "serd/memory.h"
#include "serd/statement.h"
#include "serd/status.h"

#include "memory.h"
#include "sink.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  const SerdSink* target;
  SerdNode*       subject;
  SerdNode*       predicate;
  SerdNode*       object;
  SerdNode*       graph;
  bool            inclusive;
} SerdFilterData;

static void
free_data(void* const handle)
{
  if (handle) {
    SerdFilterData* const  data      = (SerdFilterData*)handle;
    const SerdWorld* const world     = data->target->world;
    SerdAllocator* const   allocator = serd_world_allocator(world);

    serd_node_free(allocator, data->subject);
    serd_node_free(allocator, data->predicate);
    serd_node_free(allocator, data->object);
    serd_node_free(allocator, data->graph);
    serd_wfree(data->target->world, data);
  }
}

static SerdStatus
serd_filter_on_event(void* const handle, const SerdEvent* const event)
{
  const SerdFilterData* const data = (SerdFilterData*)handle;

  if (event->type == SERD_STATEMENT) {
    const bool matches = serd_statement_matches(event->statement.statement,
                                                data->subject,
                                                data->predicate,
                                                data->object,
                                                data->graph);

    if (data->inclusive == matches) {
      // Emit statement with reset flags to avoid confusing the writer
      SerdEvent out_event       = *event;
      out_event.statement.flags = 0U;
      return serd_sink_write_event(data->target, &out_event);
    }

    return SERD_SUCCESS; // Skip statement
  }

  return event->type == SERD_END ? SERD_SUCCESS
                                 : serd_sink_write_event(data->target, event);
}

SerdSink*
serd_filter_new(const SerdWorld* const world,
                const SerdSink* const  target,
                const SerdNode* const  subject,
                const SerdNode* const  predicate,
                const SerdNode* const  object,
                const SerdNode* const  graph,
                const bool             inclusive)
{
  assert(world);
  assert(target);
  assert(target->world == world);

  SerdAllocator* const  allocator = serd_world_allocator(world);
  SerdFilterData* const data =
    (SerdFilterData*)serd_wcalloc(world, 1, sizeof(SerdFilterData));

  if (!data) {
    return NULL;
  }

  data->target    = target;
  data->inclusive = inclusive;

  if (subject && serd_node_type(subject) != SERD_VARIABLE) {
    if (!(data->subject = serd_node_copy(allocator, subject))) {
      free_data(data);
      return NULL;
    }
  }

  if (predicate && serd_node_type(predicate) != SERD_VARIABLE) {
    if (!(data->predicate = serd_node_copy(allocator, predicate))) {
      free_data(data);
      return NULL;
    }
  }

  if (object && serd_node_type(object) != SERD_VARIABLE) {
    if (!(data->object = serd_node_copy(allocator, object))) {
      free_data(data);
      return NULL;
    }
  }

  if (graph && serd_node_type(graph) != SERD_VARIABLE) {
    if (!(data->graph = serd_node_copy(allocator, graph))) {
      free_data(data);
      return NULL;
    }
  }

  SerdSink* const sink =
    serd_sink_new(world, data, serd_filter_on_event, free_data);

  if (!sink) {
    free_data(data);
  }

  return sink;
}
