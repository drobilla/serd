// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node_internal.h"

#include "serd/node.h"
#include "serd/object_view.h"
#include "serd/statement_view.h"
#include "serd/token_view.h"
#include "zix/string_view.h"

#include <stdbool.h>

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

// FIXME: move
static bool
token_view_equals(const SerdTokenView lhs, const SerdTokenView rhs)
{
  return lhs.type == rhs.type && zix_string_view_equals(lhs.string, rhs.string);
}

// FIXME: move
static bool
object_view_equals(const SerdObjectView lhs, const SerdObjectView rhs)
{
  return lhs.type == rhs.type && lhs.flags == rhs.flags &&
         zix_string_view_equals(lhs.string, rhs.string) &&
         token_view_equals(lhs.meta, rhs.meta);
}

bool
serd_statement_view_equals(const SerdStatementView lhs,
                           const SerdStatementView rhs)
{
  return token_view_equals(lhs.subject, rhs.subject) &&
         token_view_equals(lhs.predicate, rhs.predicate) &&
         object_view_equals(lhs.object, rhs.object) &&
         token_view_equals(lhs.graph, rhs.graph);
}
