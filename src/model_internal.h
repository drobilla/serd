// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MODEL_INTERNAL_H
#define SERD_SRC_MODEL_INTERNAL_H

#include "statement.h"

#include <serd/cursor.h>
#include <serd/model.h>
#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <zix/attributes.h>

SerdModelCaret
serd_model_statement_caret(const SerdModel* ZIX_NONNULL     model,
                           const SerdStatement* ZIX_NONNULL statement);

SerdCursor
serd_model_find_internal(const SerdModel* ZIX_NONNULL model,
                         SerdNodeID                   s,
                         SerdNodeID                   p,
                         SerdNodeID                   o,
                         SerdNodeID                   g);

#endif // SERD_SRC_MODEL_INTERNAL_H
