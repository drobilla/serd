// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TRANSACTION_H
#define SERD_TRANSACTION_H

#include "serd/attributes.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_transaction Transaction
   @ingroup serd_storage
   @{
*/

/// An atomic transaction of operations on a model
typedef struct SerdTransactionImpl SerdTransaction;

SERD_API
SerdTransaction* SERD_ALLOCATED
serd_transaction_new(SerdModel* SERD_NONNULL model);

SERD_API
SerdStatus
serd_transaction_insert(SerdTransaction* SERD_NONNULL  transaction,
                        const SerdNode* SERD_NONNULL   s,
                        const SerdNode* SERD_NONNULL   p,
                        const SerdNode* SERD_NONNULL   o,
                        const SerdNode* SERD_NULLABLE  g,
                        const SerdCaret* SERD_NULLABLE caret);

SERD_API
SerdStatus
serd_transaction_erase(SerdTransaction* SERD_NONNULL transaction,
                       const SerdNode* SERD_NONNULL  s,
                       const SerdNode* SERD_NONNULL  p,
                       const SerdNode* SERD_NONNULL  o,
                       const SerdNode* SERD_NULLABLE g);

SERD_API
SerdStatus
serd_transaction_commit(SerdTransaction* SERD_NONNULL transaction);

SERD_API
SerdStatus
serd_transaction_abort(SerdTransaction* SERD_NONNULL transaction);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_TRANSACTION_H
