// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node_internal.h"

#include "serd/caret_view.h"
#include "serd/node.h"
#include "serd/object_view.h"
#include "serd/statement_view.h"
#include "serd/token_view.h"
#include "zix/string_view.h"

// FIXME
static const SerdTokenView no_doc   = {ZIX_STATIC_STRING(""), SERD_LITERAL};
static const SerdCaretView no_caret = {no_doc, 0U, 0U};

SerdStatementView
serd_statement_view(const SerdTokenView  subject,
                    const SerdTokenView  predicate,
                    const SerdObjectView object,
                    const SerdTokenView  graph)
{
  const SerdStatementView view = {subject, predicate, object, graph, no_caret};
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
                                  serd_node_graph_view(graph),
                                  no_caret};
  return view;
}
