// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATEMENT_VIEW_H
#define SERD_STATEMENT_VIEW_H

#include "serd/attributes.h"
#include "serd/node.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_statement_view Statement View
   @ingroup serd_streaming
   @{
*/

/// A view of a statement
typedef struct {
  const SerdNode* SERD_NONNULL  subject;
  const SerdNode* SERD_NONNULL  predicate;
  const SerdNode* SERD_NONNULL  object;
  const SerdNode* SERD_NULLABLE graph;
} SerdStatementView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATEMENT_VIEW_H
