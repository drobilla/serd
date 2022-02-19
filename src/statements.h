// Copyright 2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef STATEMENTS_H
#define STATEMENTS_H

#include "statement.h" // IWYU pragma: keep
#include "system.h"

#include "serd/caret.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>
#include <stdint.h>

#define SERD_N_PAGE_STATEMENTS \
  ((SERD_PAGE_SIZE - 2 * sizeof(uintptr_t)) / sizeof(SerdStatement))

typedef struct SerdStatementPage {
  struct SerdStatementPage* ZIX_NULLABLE next; ///< Next page in the list
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
  ZixAllocator* ZIX_NONNULL       allocator; ///< Page allocator
  SerdStatementsFlags             flags;     ///< Configuration flags
  SerdStatementPage* ZIX_NULLABLE head;      ///< First page of statements
} SerdStatements;

void
serd_statements_construct(SerdStatements* ZIX_NONNULL statements,
                          ZixAllocator* ZIX_NONNULL   allocator,
                          SerdStatementsFlags         flags);

void
serd_statements_destroy(SerdStatements* ZIX_NONNULL statements);

SerdStatement* ZIX_ALLOCATED
serd_statements_append(SerdStatements* ZIX_NONNULL   statements,
                       const SerdNode* ZIX_NONNULL   s,
                       const SerdNode* ZIX_NONNULL   p,
                       const SerdNode* ZIX_NONNULL   o,
                       const SerdNode* ZIX_NONNULL   g,
                       const SerdCaret* ZIX_NULLABLE caret);

#endif // STATEMENTS_H
