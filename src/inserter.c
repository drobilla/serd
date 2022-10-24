// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log_internal.h"
#include "model_impl.h"

#include "serd/node_args.h"
#include "serd/node_id.h"
#include "serd/nodes.h"
#include "serd/string_pair_view.h"

#include <serd/caret_view.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/field.h>
#include <serd/handler.h>
#include <serd/inserter.h>
#include <serd/log.h>
#include <serd/model.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdlib.h>

typedef struct {
  SerdModel*     model;
  const SerdEnv* env;
  char*          default_graph_string;
  SerdTokenView  default_graph;
} SerdInserterData;

static SerdNodeArgs
absolute_token_args(const SerdModel* const model,
                    const SerdEnv* const   env,
                    const SerdTokenView    token)
{
  if (token.type != SERD_CURIE && token.type != SERD_URI) {
    return serd_a_token_view(token);
  }

  SerdStringPairView uri = {zix_empty_string(), zix_empty_string()};
  if (serd_env_resolve(env, token, &uri)) {
    serd_logf(model->world,
              SERD_LOG_LEVEL_ERROR,
              serd_no_caret(),
              "failed to expand \"%s\" for insertion",
              token.string.data);
    return serd_a_null();
  }

  return serd_a_joined_uri(uri.prefix, uri.suffix);
}

static SerdNodeArgs
absolute_object_args(const SerdModel* const model,
                     const SerdEnv* const   env,
                     const SerdObjectView   object)
{
  if (object.type != SERD_LITERAL) {
    return absolute_token_args(model, env, serd_object_token_view(object));
  }

  if (!(object.flags & SERD_HAS_DATATYPE)) {
    return serd_a_object_view(object);
  }

  SerdNodeArgs     datatype_args = absolute_token_args(model, env, object.meta);
  const SerdNodeID datatype_id =
    (datatype_args.type || datatype_args.data.as_node_id)
      ? serd_nodes_id(model->nodes, datatype_args)
      : 0U;

  if (!datatype_id) {
    serd_logf(model->world,
              SERD_LOG_LEVEL_ERROR,
              serd_no_caret(),
              "failed to expand datatype \"%s\" for insertion",
              object.meta.string.data);
    return serd_a_null();
  }

  return serd_a_literal(object.string, object.flags, datatype_id);
}

static SerdStatus
serd_inserter_on_statement(SerdInserterData* const data,
                           const SerdEventFlags    flags,
                           const SerdStatementView statement,
                           const SerdCaretView     caret)
{
  (void)flags;

  SerdModel* const     model = data->model;
  const SerdEnv* const env   = data->env;

  const SerdNodeArgs s  = absolute_token_args(model, env, statement.subject);
  const SerdNodeArgs p  = absolute_token_args(model, env, statement.predicate);
  const SerdNodeArgs o  = absolute_object_args(model, env, statement.object);
  const SerdNodeArgs g  = absolute_token_args(model, env, statement.graph);
  const SerdStatus   st = serd_model_add_from(model, s, p, o, g, caret);

  return (st == SERD_FAILURE)   ? SERD_SUCCESS
         : (st == SERD_BAD_ARG) ? SERD_BAD_DATA
                                : st;
}

static SerdStatus
serd_inserter_on_event(void* const handle, const SerdEvent* const event)
{
  SerdInserterData* const data = (SerdInserterData*)handle;

  if (event->type == SERD_EVENT_STATEMENT) {
    if (!event->body.statement.graph.type && data->default_graph.type) {
      const SerdStatementView statement = {event->body.statement.subject,
                                           event->body.statement.predicate,
                                           event->body.statement.object,
                                           data->default_graph};
      return serd_inserter_on_statement(
        data, event->flags, statement, event->caret);
    }

    return serd_inserter_on_statement(
      data, event->flags, event->body.statement, event->caret);
  }

  return SERD_SUCCESS;
}

static void
destroy_data(void* const ptr)
{
  const SerdInserterData* const data = (const SerdInserterData*)ptr;
  zix_free(data->model->allocator, data->default_graph_string);
}

SerdHandler*
serd_inserter_new(SerdModel* const     model,
                  const SerdEnv* const env,
                  const SerdTokenView  default_graph)
{
  assert(model);
  assert(env);

  if (default_graph.type &&
      !serd_field_supports(SERD_GRAPH, default_graph.type)) {
    return NULL;
  }

  ZixAllocator* const allocator = serd_world_allocator(model->world);
  SerdHandler* const  inserter  = serd_handler_new(
    allocator, serd_inserter_on_event, destroy_data, sizeof(SerdInserterData));
  if (!inserter) {
    return NULL;
  }

  SerdInserterData* const data = (SerdInserterData*)serd_handler_data(inserter);

  data->model = model;
  data->env   = env;

  if (default_graph.type) {
    data->default_graph_string =
      zix_string_view_copy(allocator, default_graph.string);
    if (!data->default_graph_string) {
      serd_handler_free(inserter);
      return NULL;
    }

    data->default_graph.type          = default_graph.type;
    data->default_graph.string.data   = data->default_graph_string;
    data->default_graph.string.length = default_graph.string.length;
  }

  return inserter;
}
