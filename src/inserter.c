/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#include "model.h"
#include "statement.h"
#include "world.h"

#include "serd/serd.h"

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
        SERD_LOG_ERRORF(world,
                        SERD_ERR_BAD_ARG,
                        "attempt to insert relative URI <%s> into model",
                        serd_node_string(node));
        return false;
      }
      break;

    case SERD_CURIE:
      SERD_LOG_ERRORF(world,
                      SERD_ERR_BAD_ARG,
                      "attempt to insert prefixed name \"%s\" into model",
                      serd_node_string(node));
      return false;

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
      return SERD_ERR_BAD_ARG;
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

  const SerdCursor* const cur =
    (data->model->flags & SERD_STORE_CURSORS) ? statement->cursor : NULL;

  const SerdStatus st = serd_model_add_internal(data->model, cur, s, p, o, g);

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
  SerdInserterData* const data =
    (SerdInserterData*)calloc(1, sizeof(SerdInserterData));

  data->model         = model;
  data->default_graph = serd_node_copy(default_graph);

  SerdSink* const sink =
    serd_sink_new(data, (SerdEventFunc)serd_inserter_on_event, free);

  return sink;
}
