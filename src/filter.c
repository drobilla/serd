// Copyright 2019-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/filter.h"

#include "sink_impl.h"

#include "serd/event.h"
#include "serd/object_view.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/token_view.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

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
    SerdFilterData* const data      = (SerdFilterData*)handle;
    ZixAllocator* const   allocator = data->target->allocator;

    serd_node_free(allocator, data->subject);
    serd_node_free(allocator, data->predicate);
    serd_node_free(allocator, data->object);
    serd_node_free(allocator, data->graph);
    zix_free(allocator, data);
  }
}

static inline bool
token_matches(const SerdNode* const ZIX_NULLABLE a, const SerdTokenView b)
{
  return !a || serd_node_equals_token_view(a, b);
}

static inline bool
object_matches(const SerdNode* const ZIX_NULLABLE a, const SerdObjectView b)
{
  return !a || serd_node_equals_object_view(a, b);
}

static SerdStatus
serd_filter_on_event(void* const handle, const SerdEvent* const event)
{
  const SerdFilterData* const data = (SerdFilterData*)handle;

  if (event->type == SERD_STATEMENT) {
    const SerdStatementView statement = event->statement.statement;

    const bool matches = token_matches(data->subject, statement.subject) &&
                         token_matches(data->predicate, statement.predicate) &&
                         object_matches(data->object, statement.object) &&
                         token_matches(data->graph, statement.graph);

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

  ZixAllocator* const   alloc = serd_world_allocator(world);
  SerdFilterData* const data =
    (SerdFilterData*)zix_calloc(alloc, 1, sizeof(SerdFilterData));

  if (!data) {
    return NULL;
  }

  data->target    = target;
  data->inclusive = inclusive;

  if (subject && serd_node_type(subject) != SERD_VARIABLE) {
    if (!(data->subject = serd_node_copy(alloc, subject))) {
      free_data(data);
      return NULL;
    }
  }

  if (predicate && serd_node_type(predicate) != SERD_VARIABLE) {
    if (!(data->predicate = serd_node_copy(alloc, predicate))) {
      free_data(data);
      return NULL;
    }
  }

  if (object && serd_node_type(object) != SERD_VARIABLE) {
    if (!(data->object = serd_node_copy(alloc, object))) {
      free_data(data);
      return NULL;
    }
  }

  if (graph && serd_node_type(graph) != SERD_VARIABLE) {
    if (!(data->graph = serd_node_copy(alloc, graph))) {
      free_data(data);
      return NULL;
    }
  }

  SerdSink* const sink =
    serd_sink_new(alloc, data, serd_filter_on_event, free_data);

  if (!sink) {
    free_data(data);
  }

  return sink;
}
