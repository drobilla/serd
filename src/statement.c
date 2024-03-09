// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "statement.h"

#include "caret_impl.h" // IWYU pragma: keep
#include "match.h"
#include "warnings.h"

#include "serd/caret.h"
#include "serd/field.h"
#include "serd/node.h"
#include "serd/statement_view.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

SerdStatement*
serd_statement_new(ZixAllocator* const    allocator,
                   const SerdNode* const  s,
                   const SerdNode* const  p,
                   const SerdNode* const  o,
                   const SerdNode* const  g,
                   const SerdCaret* const caret)
{
  assert(s);
  assert(p);
  assert(o);

  if (serd_node_type(s) == SERD_LITERAL || serd_node_type(p) == SERD_LITERAL ||
      serd_node_type(p) == SERD_BLANK ||
      (g && serd_node_type(g) == SERD_LITERAL)) {
    return NULL;
  }

  SerdStatement* statement =
    (SerdStatement*)zix_malloc(allocator, sizeof(SerdStatement));

  if (statement) {
    statement->nodes[0] = s;
    statement->nodes[1] = p;
    statement->nodes[2] = o;
    statement->nodes[3] = g;
    statement->caret    = NULL;

    if (caret) {
      if (!(statement->caret = serd_caret_copy(allocator, caret))) {
        zix_free(allocator, statement);
        return NULL;
      }
    }
  }

  return statement;
}

SerdStatement*
serd_statement_copy(ZixAllocator* const        allocator,
                    const SerdStatement* const statement)
{
  if (!statement) {
    return NULL;
  }

  SerdStatement* copy =
    (SerdStatement*)zix_malloc(allocator, sizeof(SerdStatement));

  if (copy) {
    memcpy(copy, statement, sizeof(SerdStatement));

    if (statement->caret) {
      if (!(copy->caret = serd_caret_copy(allocator, statement->caret))) {
        zix_free(allocator, copy);
        return NULL;
      }
    }
  }

  return copy;
}

void
serd_statement_free(ZixAllocator* const  allocator,
                    SerdStatement* const statement)
{
  if (statement) {
    zix_free(allocator, statement->caret);
    zix_free(allocator, statement);
  }
}

SerdStatementView
serd_statement_view(const SerdStatement* const statement)
{
  const SerdNode* const s = statement->nodes[0];
  const SerdNode* const p = statement->nodes[1];
  const SerdNode* const o = statement->nodes[2];
  const SerdNode* const g = statement->nodes[3];
  assert(s);
  assert(p);
  assert(o);

  SerdStatementView view = {s, p, o, g, {NULL, 0, 0}};

  if (statement->caret) {
    view.caret.document = statement->caret->document;
    view.caret.line     = statement->caret->line;
    view.caret.column   = statement->caret->col;
  }

  return view;
}

const SerdNode*
serd_statement_node(const SerdStatement* const statement, const SerdField field)
{
  assert(statement);
  return statement->nodes[field];
}

SERD_DISABLE_NULL_WARNINGS

const SerdNode*
serd_statement_subject(const SerdStatement* const statement)
{
  assert(statement);
  return statement->nodes[SERD_SUBJECT];
}

const SerdNode*
serd_statement_predicate(const SerdStatement* const statement)
{
  assert(statement);
  return statement->nodes[SERD_PREDICATE];
}

const SerdNode*
serd_statement_object(const SerdStatement* const statement)
{
  assert(statement);
  return statement->nodes[SERD_OBJECT];
}

SERD_RESTORE_WARNINGS

const SerdNode*
serd_statement_graph(const SerdStatement* const statement)
{
  assert(statement);
  return statement->nodes[SERD_GRAPH];
}

const SerdCaret*
serd_statement_caret(const SerdStatement* const statement)
{
  assert(statement);
  return statement->caret;
}

bool
serd_statement_equals(const SerdStatement* const a,
                      const SerdStatement* const b)
{
  return (a == b || (a && b && serd_node_equals(a->nodes[0], b->nodes[0]) &&
                     serd_node_equals(a->nodes[1], b->nodes[1]) &&
                     serd_node_equals(a->nodes[2], b->nodes[2]) &&
                     serd_node_equals(a->nodes[3], b->nodes[3])));
}

bool
serd_statement_matches(const SerdStatement* const statement,
                       const SerdNode* const      subject,
                       const SerdNode* const      predicate,
                       const SerdNode* const      object,
                       const SerdNode* const      graph)
{
  assert(statement);

  return (serd_match_node(statement->nodes[0], subject) &&
          serd_match_node(statement->nodes[1], predicate) &&
          serd_match_node(statement->nodes[2], object) &&
          serd_match_node(statement->nodes[3], graph));
}
