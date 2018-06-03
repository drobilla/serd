// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STATEMENT_H
#define SERD_SRC_STATEMENT_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/node.h"

#include <stdbool.h>

struct SerdStatementImpl {
  const SerdNode* nodes[4];
  SerdCaret*      caret;
};

SERD_PURE_FUNC
bool
serd_statement_is_valid(const SerdNode* subject,
                        const SerdNode* predicate,
                        const SerdNode* object,
                        const SerdNode* graph);

#endif // SERD_SRC_STATEMENT_H
