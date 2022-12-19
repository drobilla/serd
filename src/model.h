// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MODEL_H
#define SERD_SRC_MODEL_H

#include "cursor.h"

#include "serd/cursor.h"
#include "serd/model.h"
#include "serd/nodes.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/btree.h"

#include <stddef.h>

struct SerdModelImpl {
  ZixAllocator*      allocator;     ///< Allocator for everything in this model
  SerdWorld*         world;         ///< World this model is a part of
  SerdNodes*         nodes;         ///< Interned nodes in this model
  ZixBTree*          indices[12];   ///< Trees of SerdStatement pointers
  SerdCursor         end;           ///< End cursor (always the same)
  size_t             version;       ///< Version incremented on every change
  SerdStatementOrder default_order; ///< Order for main statement-owning index
  SerdModelFlags     flags;         ///< Active indices and features
};

#endif // SERD_SRC_MODEL_H
