// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log_internal.h"
#include "model_impl.h"

#include <serd/caret_view.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/field.h>
#include <serd/handler.h>
#include <serd/inserter.h>
#include <serd/log.h>
#include <serd/model.h>
#include <serd/model_caret.h>
#include <serd/node_args.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/string_pair_view.h>
#include <serd/token_view.h>
#include <serd/tuple.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
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
  const SerdStatus   st  = serd_env_resolve(env, token, &uri);
  if (st) {
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

  assert(object.meta.type == SERD_URI || object.meta.type == SERD_CURIE);

  const SerdNodeID datatype_id =
    serd_nodes_id(model->nodes, absolute_token_args(model, env, object.meta));

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
  SerdNodes* const     nodes = serd_model_mutable_nodes(model);

  const bool has_g = serd_field_supports(SERD_GRAPH, statement.graph.type);

  const SerdNodeArgs s = absolute_token_args(model, env, statement.subject);
  const SerdNodeArgs p = absolute_token_args(model, env, statement.predicate);
  const SerdNodeArgs o = absolute_object_args(model, env, statement.object);
  const SerdNodeArgs g = absolute_token_args(model, env, statement.graph);
  if (!s.type || !p.type || !o.type || (has_g && !g.type)) {
    return SERD_BAD_DATA;
  }

  const SerdNodeID s_id = serd_nodes_id(nodes, s);
  const SerdNodeID p_id = serd_nodes_id(nodes, p);
  const SerdNodeID o_id = serd_nodes_id(nodes, o);
  const SerdNodeID g_id = serd_nodes_id(nodes, g);
  if (!s_id || !p_id || !o_id ||
      (serd_field_supports(SERD_GRAPH, statement.graph.type) && !g_id)) {
    return SERD_BAD_ALLOC;
  }

  SerdNodeID doc_id = 0U;
  if (caret.document.length) {
    doc_id = serd_nodes_id(nodes, serd_a_token(SERD_LITERAL, caret.document));
    if (!doc_id) {
      return SERD_BAD_ALLOC;
    }
  }

  const SerdTuple      tuple  = {{s_id, p_id, o_id, g_id}};
  const SerdModelCaret origin = {doc_id, caret.line, caret.column};
  const SerdStatus     st     = serd_model_insert_tuple(model, tuple, origin);

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
