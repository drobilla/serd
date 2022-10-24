// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MODEL_IMPL_H
#define SERD_SRC_MODEL_IMPL_H

#include "compare_data.h"
#include "cursor_impl.h"

#include <serd/cursor.h>
#include <serd/model.h>
#include <serd/nodes.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/btree.h>
#include <zix/hash.h>

#include <stddef.h>

struct SerdModelImpl {
  ZixAllocator* ZIX_NULLABLE allocator;     ///< Allocator for this model
  SerdWorld* ZIX_NONNULL     world;         ///< World this model is a part of
  SerdNodes* ZIX_NONNULL     nodes;         ///< Interned nodes in this model
  ZixHash* ZIX_UNSPECIFIED   carets;        ///< Statement carets
  ZixBTree* ZIX_UNSPECIFIED  indices[12];   ///< Trees of SerdStatement pointers
  CompareData                cmp_data[12];  ///< Data for tree comparator
  SerdCursor                 end;           ///< End cursor (always the same)
  size_t                     version;       ///< Version bumped for each change
  SerdStatementOrder         default_order; ///< Order of primary/owning index
  SerdModelFlags             flags;         ///< Active indices and features
};

#endif // SERD_SRC_MODEL_IMPL_H
