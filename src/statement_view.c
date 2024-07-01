// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "serd/node.h"
#include "serd/object_view.h"
#include "serd/statement_view.h"
#include "serd/token_view.h"

SerdStatementView
serd_statement_view(const SerdTokenView  subject,
                    const SerdTokenView  predicate,
                    const SerdObjectView object,
                    const SerdTokenView  graph)
{
  const SerdStatementView view = {subject, predicate, object, graph};
  return view;
}

SerdStatementView
serd_statement_view_nodes(const SerdNode* const subject,
                          const SerdNode* const predicate,
                          const SerdNode* const object,
                          const SerdNode* const graph)
{
  const SerdStatementView view = {serd_node_token_view(subject),
                                  serd_node_token_view(predicate),
                                  serd_node_object_view(object),
                                  serd_node_graph_view(graph)};
  return view;
}
