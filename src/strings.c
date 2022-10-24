// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "nodes_internal.h"

#include <serd/caret_view.h>
#include <serd/literal_view.h>
#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/statement_view.h>
#include <serd/strings.h>
#include <serd/struct_literal.h>
#include <serd/token_view.h>
#include <serd/tuple.h>
#include <zix/allocator.h>

#include <assert.h>

struct SerdStringsImpl {
  ZixAllocator*    allocator;
  const SerdNodes* nodes;
};

SerdStrings*
serd_strings_new(ZixAllocator* const allocator, const SerdNodes* const nodes)
{
  SerdStrings* const strings =
    (SerdStrings*)zix_calloc(allocator, 1, sizeof(SerdStrings));

  if (strings) {
    strings->allocator = allocator;
    strings->nodes     = nodes;
  }

  return strings;
}

void
serd_strings_free(SerdStrings* const strings)
{
  if (strings) {
    zix_free(strings->allocator, strings);
  }
}

SerdTokenView
serd_strings_token(SerdStrings* const strings, const SerdNodeID id)
{
  assert(strings);

  return serd_nodes_get_token(strings->nodes, id);
}

SerdObjectView
serd_strings_object(SerdStrings* const strings, const SerdNodeID id)
{
  assert(strings);

  return serd_nodes_get_object(strings->nodes, id);
}

SerdLiteralView
serd_strings_literal(SerdStrings* const strings, const SerdNodeID id)
{
  assert(strings);

  return serd_nodes_get_literal(strings->nodes, id);
}

SerdStatementView
serd_strings_statement(SerdStrings* const strings, const SerdTuple tuple)
{
  assert(strings);

  return SERD_STRUCT_LITERAL(
    SerdStatementView,
    serd_nodes_get_token(strings->nodes, tuple.nodes[0]),
    serd_nodes_get_token(strings->nodes, tuple.nodes[1]),
    serd_nodes_get_object(strings->nodes, tuple.nodes[2]),
    serd_nodes_get_token(strings->nodes, tuple.nodes[3]));
}

SerdCaretView
serd_strings_caret(SerdStrings* const strings, const SerdModelCaret caret)
{
  assert(strings);

  return SERD_STRUCT_LITERAL(
    SerdCaretView,
    serd_nodes_get_token(strings->nodes, caret.document).string,
    caret.line,
    caret.column);
}
