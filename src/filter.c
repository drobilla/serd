/*
  Copyright 2019-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "serd/serd.h"

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
    SerdFilterData* const      data      = (SerdFilterData*)handle;
    const SerdWorld* const     world     = data->target->world;
    const SerdAllocator* const allocator = serd_world_allocator(world);

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
      out_event.statement.flags = 0u;
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

  const SerdAllocator* const allocator = serd_world_allocator(world);
  SerdFilterData* const      data =
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

  return serd_sink_new(world, data, serd_filter_on_event, free_data);
}
