// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STATEMENT_IMPL_H
#define SERD_SRC_STATEMENT_IMPL_H

#include "serd/caret.h"
#include "serd/node.h"
#include "zix/attributes.h"

struct SerdStatementImpl {
  const SerdNode* ZIX_NULLABLE nodes[4];
  SerdCaret* ZIX_NULLABLE      caret;
};

#endif // SERD_SRC_STATEMENT_IMPL_H
