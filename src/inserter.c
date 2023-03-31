// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log.h"
#include "memory.h"
#include "model.h"

#include "serd/caret_view.h"
#include "serd/event.h"
#include "serd/field.h"
#include "serd/inserter.h"
#include "serd/log.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/object_view.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/token_view.h"
#include "serd/uri.h"
#include "serd/world.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  SerdModel* model;
  SerdNode*  default_graph;
} SerdInserterData;

static bool
can_insert_token(SerdWorld* const world, const SerdTokenView node)
{
  switch (node.type) {
  case SERD_LITERAL:
    break;

  case SERD_URI:
    if (!serd_uri_string_has_scheme(node.string.data)) {
      serd_logf(world,
                SERD_LOG_LEVEL_ERROR,
                "attempt to insert relative URI <%s> into model",
                node.string.data);
      return false;
    }
    break;

  case SERD_CURIE:
    return false;

  case SERD_BLANK:
  case SERD_VARIABLE:
    break;
  }

  return true;
}

static bool
can_insert_object(SerdWorld* const world, const SerdObjectView node)
{
  if (node.type == SERD_LITERAL) {
    return !(node.flags & SERD_HAS_DATATYPE) ||
           can_insert_token(world, node.meta);
  }

  const SerdTokenView token = {node.string, node.type};
  return can_insert_token(world, token);
}

static const SerdNode*
intern_object_view(SerdNodes* const nodes, const SerdObjectView object)
{
  if (object.type != SERD_LITERAL || !object.flags) {
    return serd_nodes_get(nodes, serd_a_token(object.type, object.string));
  }

  const SerdNode* const meta =
    (object.flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE))
      ? serd_nodes_get(nodes, serd_a_token_view(object.meta))
      : NULL;

  return serd_nodes_get(nodes,
                        serd_a_literal(object.string, object.flags, meta));
}

static SerdStatus
serd_inserter_on_statement(SerdInserterData* const       data,
                           const SerdStatementEventFlags flags,
                           const SerdStatementView       statement,
                           const SerdCaretView           caret)
{
  (void)flags;

  SerdModel* const model = data->model;
  SerdWorld* const world = model->world;

  // Check that every node is expanded (so its string is context-free)
  if (!can_insert_token(world, statement.subject) ||
      !can_insert_token(world, statement.predicate) ||
      !can_insert_object(world, statement.object) ||
      (serd_field_supports(SERD_GRAPH, statement.graph.type) &&
       !can_insert_token(world, statement.graph))) {
    return SERD_BAD_DATA;
  }

  const SerdNode* const s =
    serd_nodes_get(model->nodes, serd_a_token_view(statement.subject));

  const SerdNode* const p =
    serd_nodes_get(model->nodes, serd_a_token_view(statement.predicate));

  const SerdNode* const o = intern_object_view(model->nodes, statement.object);

  const SerdNode* const g =
    serd_field_supports(SERD_GRAPH, statement.graph.type)
      ? serd_nodes_get(model->nodes, serd_a_token_view(statement.graph))
      : NULL;

  const SerdStatus st = (data->model->flags & SERD_STORE_CARETS)
                          ? serd_model_add_from(data->model, s, p, o, g, caret)
                          : serd_model_add(data->model, s, p, o, g);

  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
serd_inserter_on_event(void* const handle, const SerdEvent* const event)
{
  SerdInserterData* const data = (SerdInserterData*)handle;

  if (event->type == SERD_STATEMENT) {
    return serd_inserter_on_statement(data,
                                      event->statement.flags,
                                      event->statement.statement,
                                      event->statement.caret);
  }

  return SERD_SUCCESS;
}

static SerdInserterData*
serd_inserter_data_new(SerdModel* const      model,
                       const SerdNode* const default_graph)
{
  SerdInserterData* const data =
    (SerdInserterData*)serd_wcalloc(model->world, 1, sizeof(SerdInserterData));

  if (data) {
    data->model         = model;
    data->default_graph = serd_node_copy(model->allocator, default_graph);
  }

  return data;
}

static void
serd_inserter_data_free(void* const ptr)
{
  SerdInserterData* const data = (SerdInserterData*)ptr;
  serd_node_free(data->model->allocator, data->default_graph);
  serd_wfree(data->model->world, data);
}

SerdSink*
serd_inserter_new(SerdModel* const model, const SerdNode* const default_graph)
{
  assert(model);

  SerdEventFunc           func = serd_inserter_on_event;
  SerdInserterData* const data = serd_inserter_data_new(model, default_graph);

  return data ? serd_sink_new(serd_world_allocator(model->world),
                              data,
                              func,
                              (SerdFreeFunc)serd_inserter_data_free)
              : NULL;
}
