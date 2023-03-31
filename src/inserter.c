// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log.h"
#include "memory.h"
#include "model.h"

#include "serd/caret_view.h"
#include "serd/event.h"
#include "serd/inserter.h"
#include "serd/log.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
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
can_insert(SerdWorld* const world, const SerdNode* const node)
{
  if (node) {
    switch (serd_node_type(node)) {
    case SERD_LITERAL:
      return can_insert(world, serd_node_datatype(node));

    case SERD_URI:
      if (!serd_uri_string_has_scheme(serd_node_string(node))) {
        serd_logf(world,
                  SERD_LOG_LEVEL_ERROR,
                  "attempt to insert relative URI <%s> into model",
                  serd_node_string(node));
        return false;
      }
      break;

    case SERD_CURIE:
      return false;

    case SERD_BLANK:
    case SERD_VARIABLE:
      break;
    }
  }

  return true;
}

static SerdStatus
serd_inserter_on_statement(SerdInserterData* const       data,
                           const SerdStatementEventFlags flags,
                           const SerdStatementView       statement)
{
  static const SerdCaretView no_caret = {NULL, 0, 0};

  (void)flags;

  SerdModel* const model = data->model;
  SerdWorld* const world = model->world;

  // Check that every node is expanded so it is context-free
  if (!can_insert(world, statement.subject) ||
      !can_insert(world, statement.predicate) ||
      !can_insert(world, statement.object) ||
      !can_insert(world, statement.graph)) {
    return SERD_BAD_DATA;
  }

  const SerdNode* const s = serd_nodes_intern(model->nodes, statement.subject);

  const SerdNode* const p =
    serd_nodes_intern(model->nodes, statement.predicate);

  const SerdNode* const o = serd_nodes_intern(model->nodes, statement.object);

  const SerdNode* const g = serd_nodes_intern(
    model->nodes, statement.graph ? statement.graph : data->default_graph);

  const SerdStatus st =
    (data->model->flags & SERD_STORE_CARETS)
      ? serd_model_add_from(data->model, s, p, o, g, statement.caret)
      : serd_model_add_from(data->model, s, p, o, g, no_caret);

  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
serd_inserter_on_event(void* const handle, const SerdEvent* const event)
{
  SerdInserterData* const data = (SerdInserterData*)handle;

  if (event->type == SERD_STATEMENT) {
    return serd_inserter_on_statement(
      data, event->statement.flags, event->statement.statement);
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
