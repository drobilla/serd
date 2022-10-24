// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MODEL_INTERNAL_H
#define SERD_SRC_MODEL_INTERNAL_H

#include "statement.h"

#include <serd/caret_view.h>
#include <serd/cursor.h>
#include <serd/model.h>
#include <serd/node_id.h>
#include <zix/attributes.h>

#include <stdbool.h>
#include <stddef.h>

SerdCaretView
serd_model_statement_caret(const SerdModel* ZIX_NONNULL     model,
                           const SerdStatement* ZIX_NONNULL statement);

SerdCursor
serd_model_search_by_id(const SerdModel* ZIX_NONNULL model,
                        SerdNodeID                   s,
                        SerdNodeID                   p,
                        SerdNodeID                   o,
                        SerdNodeID                   g);

const SerdStatement* ZIX_NULLABLE
serd_model_get_statement_by_id(const SerdModel* ZIX_NONNULL model,
                               SerdNodeID                   s,
                               SerdNodeID                   p,
                               SerdNodeID                   o,
                               SerdNodeID                   g);

bool
serd_model_ask_by_id(const SerdModel* ZIX_NONNULL model,
                     SerdNodeID                   s,
                     SerdNodeID                   p,
                     SerdNodeID                   o,
                     SerdNodeID                   g);

size_t
serd_model_count_by_id(const SerdModel* ZIX_NONNULL model,
                       SerdNodeID                   s,
                       SerdNodeID                   p,
                       SerdNodeID                   o,
                       SerdNodeID                   g);

#endif // SERD_SRC_MODEL_INTERNAL_H
