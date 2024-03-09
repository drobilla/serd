// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATEMENT_VIEW_H
#define SERD_STATEMENT_VIEW_H

#include "serd/attributes.h"
#include "serd/caret_view.h"
#include "serd/node.h"

SERD_BEGIN_DECLS

/**
   @ingroup serd_streaming
   @{
*/

typedef struct {
  const SerdNode* SERD_NONNULL  subject;
  const SerdNode* SERD_NONNULL  predicate;
  const SerdNode* SERD_NONNULL  object;
  const SerdNode* SERD_NULLABLE graph;
  SerdCaretView                 caret;
} SerdStatementView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATEMENT_VIEW_H
