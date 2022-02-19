// Copyright 2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef STATEMENTS_H
#define STATEMENTS_H

#include "statement.h"
#include "system.h"

#include "serd/serd.h"

#include <stddef.h>
#include <stdint.h>

#define SERD_N_PAGE_STATEMENTS \
  ((SERD_PAGE_SIZE - 2 * sizeof(uintptr_t)) / sizeof(SerdStatement))

typedef struct SerdStatementPage {
  struct SerdStatementPage* SERD_NULLABLE next; ///< Next page in the list
  size_t count; ///< Number of statements in this page

  SerdStatement statements[SERD_N_PAGE_STATEMENTS];
} SerdStatementPage;

typedef size_t SerdStatementsFlags;

/**
   Storage for statements in a model.

   This provides an interface for adding individual statements (and getting an
   interned pointer), and deleting all statements at once.  Statements are
   stored in pages for efficiency, and pages form a linked list so that
   everything can be traversed on destruction.

   There is currently no facility for freeing individual statements and reusing
   their memory, or finding statements.  In other words, this is essentially an
   append-only log of added statements, but only in memory.
*/
typedef struct {
  SerdAllocator* SERD_NONNULL      allocator; ///< Page allocator
  SerdStatementsFlags              flags;     ///< Configuration flags
  SerdStatementPage* SERD_NULLABLE head;      ///< First page of statements
} SerdStatements;

void
serd_statements_construct(SerdStatements* SERD_NONNULL statements,
                          SerdAllocator* SERD_NONNULL  allocator,
                          SerdStatementsFlags          flags);

void
serd_statements_destroy(SerdStatements* SERD_NONNULL statements);

SerdStatement* SERD_ALLOCATED
serd_statements_append(SerdStatements* SERD_NONNULL   statements,
                       const SerdNode* SERD_NONNULL   s,
                       const SerdNode* SERD_NONNULL   p,
                       const SerdNode* SERD_NONNULL   o,
                       const SerdNode* SERD_NONNULL   g,
                       const SerdCaret* SERD_NULLABLE caret);

#endif // STATEMENTS_H
