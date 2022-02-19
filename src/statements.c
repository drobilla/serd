// Copyright 2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "statements.h"

#include <assert.h>
#include <string.h>

void
serd_statements_construct(SerdStatements* const     statements,
                          ZixAllocator* const       allocator,
                          const SerdStatementsFlags flags)
{
  assert(allocator);

  memset(statements, 0, sizeof(*statements));
  statements->allocator = allocator;
  statements->flags     = flags;
}

void
serd_statements_destroy(SerdStatements* const statements)
{
  while (statements->head) {
    SerdStatementPage* const next = statements->head->next;

    zix_free(statements->allocator, statements->head);
    statements->head = next;
  }

  memset(statements, 0, sizeof(*statements));
}

SerdStatement*
serd_statements_append(SerdStatements* const  statements,
                       const SerdNode* const  s,
                       const SerdNode* const  p,
                       const SerdNode* const  o,
                       const SerdNode* const  g,
                       const SerdCaret* const caret)
{
  // Allocate a new head page if necessary
  if (!statements->head || statements->head->count >= SERD_N_PAGE_STATEMENTS) {
    SerdStatementPage* const new_head = (SerdStatementPage*)zix_calloc(
      statements->allocator, 1, sizeof(SerdStatementPage));

    new_head->next   = statements->head;
    statements->head = new_head;
  }

  // Append new statement to the head page
  SerdStatementPage* const head  = statements->head;
  SerdStatement* const statement = &head->statements[statements->head->count++];

  statement->nodes[0] = s;
  statement->nodes[1] = p;
  statement->nodes[2] = o;
  statement->nodes[3] = g;

  if ((statements->flags & SERD_STORE_CARETS) && caret) {
    statement->caret = *caret;
  }

  return statement;
}
