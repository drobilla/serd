// Copyright 2019-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/filter.h"

#include "serd/event.h"
#include "serd/statement.h"
#include "serd/status.h"

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
    SerdFilterData* data = (SerdFilterData*)handle;

    serd_node_free(data->subject);
    serd_node_free(data->predicate);
    serd_node_free(data->object);
    serd_node_free(data->graph);
    free(data);
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
serd_filter_new(const SerdSink* const target,
                const SerdNode* const subject,
                const SerdNode* const predicate,
                const SerdNode* const object,
                const SerdNode* const graph,
                const bool            inclusive)
{
  assert(target);

  SerdFilterData* const data =
    (SerdFilterData*)calloc(1, sizeof(SerdFilterData));

  data->target    = target;
  data->inclusive = inclusive;

  if (subject && serd_node_type(subject) != SERD_VARIABLE) {
    data->subject = serd_node_copy(subject);
  }

  if (predicate && serd_node_type(predicate) != SERD_VARIABLE) {
    data->predicate = serd_node_copy(predicate);
  }

  if (object && serd_node_type(object) != SERD_VARIABLE) {
    data->object = serd_node_copy(object);
  }

  if (graph && serd_node_type(graph) != SERD_VARIABLE) {
    data->graph = serd_node_copy(graph);
  }

  return serd_sink_new(data, serd_filter_on_event, free_data);
}
