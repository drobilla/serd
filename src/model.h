// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MODEL_H
#define SERD_SRC_MODEL_H

#include "cursor.h"

#include "serd/caret_view.h"
#include "serd/cursor.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/world.h"
#include "statement.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/btree.h"

#include <stddef.h>

/// A view of a statement inside a model
typedef struct {
  const SerdNode* ZIX_NONNULL  subject;
  const SerdNode* ZIX_NONNULL  predicate;
  const SerdNode* ZIX_NONNULL  object;
  const SerdNode* ZIX_NULLABLE graph;
  SerdCaretView                caret;
} SerdModelStatementView;

struct SerdModelImpl {
  ZixAllocator* ZIX_NULLABLE allocator;     ///< Allocator for this model
  SerdWorld* ZIX_NONNULL     world;         ///< World this model is a part of
  SerdNodes* ZIX_NONNULL     nodes;         ///< Interned nodes in this model
  ZixBTree* ZIX_UNSPECIFIED  indices[12];   ///< Trees of SerdStatement pointers
  SerdCursor                 end;           ///< End cursor (always the same)
  size_t                     version;       ///< Version bumped for each change
  SerdStatementOrder         default_order; ///< Order of primary/owning index
  SerdModelFlags             flags;         ///< Active indices and features
};

const SerdStatement* ZIX_NULLABLE
serd_model_get_statement_internal(const SerdModel* ZIX_NONNULL model,
                                  const SerdNode* ZIX_NULLABLE s,
                                  const SerdNode* ZIX_NULLABLE p,
                                  const SerdNode* ZIX_NULLABLE o,
                                  const SerdNode* ZIX_NULLABLE g);

#endif // SERD_SRC_MODEL_H
