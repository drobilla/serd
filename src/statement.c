// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "statement.h"

#include "caret.h"
#include "node.h"

#include "serd/statement.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool
is_resource(const SerdNode* const node)
{
  const SerdNodeType type = serd_node_type(node);

  return type == SERD_URI || type == SERD_BLANK || type == SERD_VARIABLE;
}

bool
serd_statement_is_valid(const SerdNode* const subject,
                        const SerdNode* const predicate,
                        const SerdNode* const object,
                        const SerdNode* const graph)
{
  (void)object;

  return is_resource(subject) && is_resource(predicate) &&
         serd_node_type(predicate) != SERD_BLANK &&
         (!graph || is_resource(graph));
}

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

  if (!serd_statement_is_valid(s, p, o, g)) {
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
      if (!(copy->caret =
              (SerdCaret*)zix_malloc(allocator, sizeof(SerdCaret)))) {
        zix_free(allocator, copy);
        return NULL;
      }

      memcpy(copy->caret, statement->caret, sizeof(SerdCaret));
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

const SerdNode*
serd_statement_node(const SerdStatement* const statement, const SerdField field)
{
  assert(statement);
  return statement->nodes[field];
}

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

  return (serd_node_pattern_match(statement->nodes[0], subject) &&
          serd_node_pattern_match(statement->nodes[1], predicate) &&
          serd_node_pattern_match(statement->nodes[2], object) &&
          serd_node_pattern_match(statement->nodes[3], graph));
}
