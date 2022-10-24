// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_CURSOR_IMPL_H
#define SERD_SRC_CURSOR_IMPL_H

#include <serd/model.h>
#include <serd/node_id.h>
#include <zix/btree.h>

#include <stddef.h>

/// An iteration mode that determines what to skip and when to end
typedef enum {
  SCAN_EVERYTHING,   ///< Iterate over entire store
  SCAN_RANGE,        ///< Iterate over range with equal prefix
  FILTER_EVERYTHING, ///< Iterate to end of store, filtering
  FILTER_RANGE,      ///< Iterate over range with equal prefix, filtering
} ScanMode;

/// A strategy for searching and iterating over a statement index
typedef struct {
  ScanMode mode : 4;            ///< Iteration mode
  unsigned n_prefix : 4;        ///< Number of prefix nodes that match the index
  SerdStatementOrder order : 8; ///< Order of index to scan
} ScanStrategy;

struct SerdCursorImpl {
  const SerdModel* model;      ///< Model being iterated over
  size_t           version;    ///< Model version when iterator was created
  SerdNodeID       pattern[4]; ///< Search pattern (nodes in model or null)
  ScanStrategy     strategy;   ///< Index scanning strategy
  ZixBTreeIter     iter;       ///< Current position in index
};

#endif // SERD_SRC_CURSOR_IMPL_H
