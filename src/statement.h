// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STATEMENT_H
#define SERD_SRC_STATEMENT_H

#include "serd/caret.h"
#include "serd/node.h"
#include "zix/attributes.h"

#include <stdbool.h>

struct SerdStatementImpl {
  const SerdNode* ZIX_NULLABLE nodes[4];
  SerdCaret* ZIX_NULLABLE      caret;
};

ZIX_PURE_FUNC bool
serd_statement_is_valid(const SerdNode* ZIX_NULLABLE subject,
                        const SerdNode* ZIX_NULLABLE predicate,
                        const SerdNode* ZIX_NULLABLE object,
                        const SerdNode* ZIX_NULLABLE graph);

#endif // SERD_SRC_STATEMENT_H
