// Copyright 2019-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/filter.h>

#include <serd/env.h>
#include <serd/event.h>
#include <serd/handler.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  ZixAllocator*     allocator;
  const SerdEnv*    env;
  const SerdSink*   target;
  char*             graph;
  char*             subject;
  char*             predicate;
  char*             object;
  char*             meta;
  SerdStatementView pattern;
  bool              inclusive;
} SerdFilterData;

static void
destroy_data(void* const handle)
{
  if (handle) {
    const SerdFilterData* const data      = (const SerdFilterData*)handle;
    ZixAllocator* const         allocator = data->allocator;

    zix_free(allocator, data->graph);
    zix_free(allocator, data->subject);
    zix_free(allocator, data->predicate);
    zix_free(allocator, data->object);
    zix_free(allocator, data->meta);
  }
}

static inline bool
token_matches(const SerdEnv* const env,
              const SerdTokenView  pattern,
              const SerdTokenView  node)
{
  return !pattern.type || serd_env_tokens_equal(env, pattern, node);
}

static inline bool
object_matches(const SerdEnv* const env,
               const SerdObjectView pattern,
               const SerdObjectView node)
{
  return !pattern.type || serd_env_objects_equal(env, pattern, node);
}

static SerdStatus
serd_filter_on_event(void* const handle, const SerdEvent* const event)
{
  const SerdFilterData* const data = (SerdFilterData*)handle;
  const SerdEnv* const        env  = data->env;

  if (event->type == SERD_EVENT_STATEMENT) {
    const SerdStatementView statement = event->body.statement;
    const SerdStatementView pattern   = data->pattern;

    const bool matches =
      token_matches(env, pattern.subject, statement.subject) &&
      token_matches(env, pattern.predicate, statement.predicate) &&
      object_matches(env, pattern.object, statement.object) &&
      token_matches(env, pattern.graph, statement.graph);

    if (data->inclusive == matches) {
      // Emit statement with reset flags to avoid confusing the writer
      SerdEvent out_event = *event;
      out_event.flags     = 0U;
      return serd_sink_event(data->target, out_event);
    }

    return SERD_SUCCESS; // Skip statement
  }

  return event->type == SERD_EVENT_END ? SERD_SUCCESS
                                       : serd_sink_event(data->target, *event);
}

SerdHandler*
serd_filter_new(const SerdWorld* const world,
                const SerdEnv* const   env,
                const SerdSink* const  target,
                const SerdTokenView    subject,
                const SerdTokenView    predicate,
                const SerdObjectView   object,
                const SerdTokenView    graph,
                const bool             inclusive)
{
  assert(world);
  assert(env);
  assert(target);

  ZixAllocator* const allocator = serd_world_allocator(world);
  SerdHandler* const  filter    = serd_handler_new(
    allocator, serd_filter_on_event, destroy_data, sizeof(SerdFilterData));
  if (!filter) {
    return NULL;
  }

  SerdFilterData* const    data    = (SerdFilterData*)serd_handler_data(filter);
  SerdStatementView* const pattern = &data->pattern;

  data->allocator = allocator;
  data->env       = env;
  data->target    = target;
  data->inclusive = inclusive;

  if (subject.type != SERD_VARIABLE) {
    if (!(data->subject = zix_string_view_copy(allocator, subject.string))) {
      serd_handler_free(filter);
      return NULL;
    }

    pattern->subject = serd_token_view(subject.type, zix_string(data->subject));
  }

  if (predicate.type != SERD_VARIABLE) {
    if (!(data->predicate =
            zix_string_view_copy(allocator, predicate.string))) {
      serd_handler_free(filter);
      return NULL;
    }
    pattern->predicate =
      serd_token_view(predicate.type, zix_string(data->predicate));
  }

  if (object.type != SERD_VARIABLE) {
    if (!(data->object = zix_string_view_copy(allocator, object.string)) ||
        !(data->meta = zix_string_view_copy(allocator, object.meta.string))) {
      serd_handler_free(filter);
      return NULL;
    }

    pattern->object = serd_object_view(
      object.type,
      zix_string(data->object),
      object.flags,
      serd_token_view(object.meta.type, zix_string(data->meta)));
  }

  if (graph.type != SERD_VARIABLE) {
    if (!(data->graph = zix_string_view_copy(allocator, graph.string))) {
      serd_handler_free(filter);
      return NULL;
    }
    pattern->graph = serd_token_view(graph.type, zix_string(data->graph));
  }

  return filter;
}
