// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CURSOR_H
#define SERD_CURSOR_H

#include "statement.h"

#include "serd/serd.h"
#include "zix/btree.h"

#include <stdbool.h>
#include <stddef.h>

#define N_STATEMENT_ORDERS 12u

/// An iteration mode that determines what to skip and when to end
typedef enum {
  SCAN_EVERYTHING,   ///< Iterate over entire store
  SCAN_RANGE,        ///< Iterate over range with equal prefix
  FILTER_EVERYTHING, ///< Iterate to end of store, filtering
  FILTER_RANGE,      ///< Iterate over range with equal prefix, filtering
} ScanMode;

/// A strategy for searching and iterating over a statement index
typedef struct {
  ScanMode           mode;     ///< Iteration mode
  unsigned           n_prefix; ///< Number of prefix nodes that match the index
  SerdStatementOrder order;    ///< Order of index to scan
} ScanStrategy;

struct SerdCursorImpl {
  const SerdModel* model;      ///< Model being iterated over
  const SerdNode*  pattern[4]; ///< Search pattern (nodes in model or null)
  size_t           version;    ///< Model version when iterator was created
  ZixBTreeIter     iter;       ///< Current position in index
  ScanStrategy     strategy;   ///< Index scanning strategy
};

/// Lookup table of ordered indices for each SerdStatementOrder
static const unsigned orderings[N_STATEMENT_ORDERS][4] = {
  {0u, 1u, 2u, 3u}, // SPOG
  {0u, 2u, 1u, 3u}, // SOPG
  {2u, 1u, 0u, 3u}, // OPSG
  {2u, 0u, 1u, 3u}, // OPSG
  {1u, 0u, 2u, 3u}, // PSOG
  {1u, 2u, 0u, 3u}, // PSOG
  {3u, 0u, 1u, 2u}, // GSPO
  {3u, 0u, 2u, 1u}, // GSPO
  {3u, 2u, 1u, 0u}, // GOPS
  {3u, 2u, 0u, 1u}, // GOPS
  {3u, 1u, 0u, 2u}, // GPSO
  {3u, 1u, 2u, 0u}  // GPSO
};

SerdCursor
serd_cursor_make(const SerdModel* model,
                 ZixBTreeIter     iter,
                 const SerdQuad   pattern,
                 ScanStrategy     strategy);

SerdStatus
serd_cursor_scan_next(SerdCursor* cursor);

bool
serd_iter_in_range(ZixBTreeIter   iter,
                   const SerdQuad pattern,
                   ScanStrategy   strategy);

#endif // SERD_CURSOR_H
