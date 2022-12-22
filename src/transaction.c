// Copyright 2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "macros.h"
#include "memory.h"
#include "model.h"
#include "statement.h"
#include "statements.h"

#include "serd/caret.h"
#include "serd/cursor.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/transaction.h"

#include <assert.h>
#include <stddef.h>

typedef enum {
  SERD_INSERT_STATEMENT      = 0x5D0,
  SERD_INSERT_STATEMENT_FROM = 0x5D1,
  SERD_ERASE_STATEMENT       = 0x5D2,
} SerdOperationType;

typedef struct {
  struct SerdStatementImpl statement;
} SerdInsertStatementOperationData;

typedef struct {
  struct SerdStatementImpl statement;
  SerdCaret                caret;
} SerdInsertStatementFromOperationData;

typedef struct {
  SerdCursor* cursor;
} SerdEraseStatementOperationData;

typedef struct {
  SerdOperationType type;
  union {
    SerdStatement* statement;
    SerdCursor*    cursor;
  } data;
} SerdOperation;

struct SerdTransactionImpl {
  SerdModel*     model;
  size_t         count;
  SerdOperation* operations;
};

SerdTransaction*
serd_transaction_new(SerdModel* const model)
{
  SerdTransaction* const transaction =
    (SerdTransaction*)zix_calloc(model->allocator, 1, sizeof(SerdTransaction));

  if (transaction) {
    transaction->model = model;
  }

  return transaction;
}

static SerdStatus
push_operation(SerdTransaction* const transaction, const SerdOperation op)
{
  SerdModel*           model          = transaction->model;
  SerdOperation* const new_operations = (SerdOperation*)zix_realloc(
    model->allocator, transaction->operations, transaction->count + 1);

  if (!new_operations) {
    return SERD_BAD_ALLOC;
  }

  new_operations[transaction->count] = op;
  transaction->operations            = new_operations;

  ++transaction->count;
  return SERD_SUCCESS;
}

SerdStatus
serd_transaction_insert(SerdTransaction* const transaction,
                        const SerdNode* const  s,
                        const SerdNode* const  p,
                        const SerdNode* const  o,
                        const SerdNode* const  g,
                        const SerdCaret* const caret)
{
  assert(transaction);
  assert(s);
  assert(p);
  assert(o);

  SerdModel* model = transaction->model;

  if (!serd_statement_is_valid(s, p, o, g)) {
    return SERD_BAD_ARG;
  }

  if (serd_model_ask(model, s, p, o, g)) {
    return SERD_FAILURE;
  }

  SerdStatement* const statement =
    serd_statements_append(&model->statements, s, p, o, g, caret);

  if (!statement) {
    return SERD_BAD_ALLOC;
  }

  SerdOperation op  = {SERD_INSERT_STATEMENT, {NULL}};
  op.data.statement = statement;

  return push_operation(transaction, op);
}

SerdStatus
serd_transaction_erase(SerdTransaction* const transaction,
                       const SerdNode* const  s,
                       const SerdNode* const  p,
                       const SerdNode* const  o,
                       const SerdNode* const  g)
{
  ZixAllocator* const allocator = transaction->model->allocator;
  SerdModel* const    model     = transaction->model;
  SerdCursor* const   cursor    = serd_model_find(allocator, model, s, p, o, g);
  if (serd_cursor_is_end(cursor)) {
    serd_cursor_free(allocator, cursor);
    return SERD_FAILURE;
  }

  SerdOperation op = {SERD_ERASE_STATEMENT, {NULL}};
  op.data.cursor   = cursor;

  return push_operation(transaction, op);
}

static void
serd_transaction_free(SerdTransaction* const transaction)
{
  for (size_t i = 0; i < transaction->count; ++i) {
    const SerdOperation* const op = &transaction->operations[i];
    switch (op->type) {
    case SERD_INSERT_STATEMENT:
      break;
    case SERD_INSERT_STATEMENT_FROM:
      break;
    case SERD_ERASE_STATEMENT:
      serd_cursor_free(transaction->model->allocator, op->data.cursor);
      break;
    }
  }

  zix_free(transaction->model->allocator, transaction->operations);
  zix_free(transaction->model->allocator, transaction);
}

SerdStatus
serd_transaction_commit(SerdTransaction* const transaction)
{
  SerdStatus st = SERD_SUCCESS;

  for (size_t i = 0; i < transaction->count; ++i) {
    const SerdOperation* const op = &transaction->operations[i];
    switch (op->type) {
    case SERD_INSERT_STATEMENT:
      st = MAX(st, serd_model_insert(transaction->model, op->data.statement));
      break;
    case SERD_ERASE_STATEMENT:
      st = MAX(st, serd_model_erase(transaction->model, op->data.cursor));
      break;
    }
  }

  serd_transaction_free(transaction);
  return st;
}

SerdStatus
serd_transaction_abort(SerdTransaction* const transaction)
{
  serd_transaction_free(transaction);
  return SERD_SUCCESS;
}
