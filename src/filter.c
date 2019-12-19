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

    if (data->inclusive != matches) {
      return SERD_SUCCESS;
    }
  }

  return serd_sink_write_event(data->target, event);
}

SerdSink*
serd_filter_new(const SerdSink* const target,
                const SerdNode* const subject,
                const SerdNode* const predicate,
                const SerdNode* const object,
                const SerdNode* const graph,
                const bool            inclusive)
{
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
