// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "model.h"
#include "statement.h"

#include "serd/caret.h"
#include "serd/event.h"
#include "serd/inserter.h"
#include "serd/log.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/uri.h"
#include "serd/world.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  SerdModel*      model;
  const SerdNode* default_graph;
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

    case SERD_BLANK:
    case SERD_VARIABLE:
      break;
    }
  }

  return true;
}

static SerdStatus
serd_inserter_on_statement(SerdInserterData* const    data,
                           const SerdStatementFlags   flags,
                           const SerdStatement* const statement)
{
  (void)flags;

  SerdModel* const model = data->model;
  SerdWorld* const world = model->world;

  // Check that every node is expanded so it is context-free
  for (unsigned i = 0; i < 4; ++i) {
    if (!can_insert(world, serd_statement_node(statement, (SerdField)i))) {
      return SERD_BAD_DATA;
    }
  }

  const SerdNode* const s =
    serd_nodes_intern(model->nodes, serd_statement_subject(statement));

  const SerdNode* const p =
    serd_nodes_intern(model->nodes, serd_statement_predicate(statement));

  const SerdNode* const o =
    serd_nodes_intern(model->nodes, serd_statement_object(statement));

  const SerdNode* const g = serd_nodes_intern(
    model->nodes,
    serd_statement_graph(statement) ? serd_statement_graph(statement)
                                    : data->default_graph);

  const SerdCaret* const caret =
    (data->model->flags & SERD_STORE_CARETS) ? statement->caret : NULL;

  const SerdStatus st =
    serd_model_add_with_caret(data->model, s, p, o, g, caret);

  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
serd_inserter_on_event(SerdInserterData* const data,
                       const SerdEvent* const  event)
{
  if (event->type == SERD_STATEMENT) {
    return serd_inserter_on_statement(
      data, event->statement.flags, event->statement.statement);
  }

  return SERD_SUCCESS;
}

SerdSink*
serd_inserter_new(SerdModel* const model, const SerdNode* const default_graph)
{
  assert(model);

  SerdInserterData* const data =
    (SerdInserterData*)calloc(1, sizeof(SerdInserterData));

  data->model         = model;
  data->default_graph = serd_node_copy(default_graph);

  SerdSink* const sink = serd_sink_new(
    model->world, data, (SerdEventFunc)serd_inserter_on_event, free);

  return sink;
}
