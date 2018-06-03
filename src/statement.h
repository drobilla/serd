// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STATEMENT_H
#define SERD_SRC_STATEMENT_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/node.h"

struct SerdStatementImpl {
  const SerdNode* SERD_NULLABLE nodes[4];
  SerdCaret* SERD_NULLABLE      caret;
};

#endif // SERD_SRC_STATEMENT_H
