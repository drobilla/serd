// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TRANSACTION_H
#define SERD_TRANSACTION_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/status.h"
#include "zix/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_transaction Transactions
   @ingroup serd_storage
   @{
*/

/// An atomic transaction of operations on a model
typedef struct SerdTransactionImpl SerdTransaction;

SERD_API SerdTransaction* ZIX_ALLOCATED
serd_transaction_new(SerdModel* ZIX_NONNULL model);

SERD_API SerdStatus
serd_transaction_insert(SerdTransaction* ZIX_NONNULL  transaction,
                        const SerdNode* ZIX_NONNULL   s,
                        const SerdNode* ZIX_NONNULL   p,
                        const SerdNode* ZIX_NONNULL   o,
                        const SerdNode* ZIX_NULLABLE  g,
                        const SerdCaret* ZIX_NULLABLE caret);

SERD_API SerdStatus
serd_transaction_erase(SerdTransaction* ZIX_NONNULL transaction,
                       const SerdNode* ZIX_NONNULL  s,
                       const SerdNode* ZIX_NONNULL  p,
                       const SerdNode* ZIX_NONNULL  o,
                       const SerdNode* ZIX_NULLABLE g);

SERD_API SerdStatus
serd_transaction_commit(SerdTransaction* ZIX_NONNULL transaction);

SERD_API SerdStatus
serd_transaction_abort(SerdTransaction* ZIX_NONNULL transaction);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_TRANSACTION_H
