// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_CURSOR_H
#define SERD_SRC_CURSOR_H

#include "statement.h"

#include "serd/cursor.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/status.h"
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
  {0U, 1U, 2U, 3U}, // SPOG
  {0U, 2U, 1U, 3U}, // SOPG
  {2U, 1U, 0U, 3U}, // OPSG
  {2U, 0U, 1U, 3U}, // OPSG
  {1U, 0U, 2U, 3U}, // PSOG
  {1U, 2U, 0U, 3U}, // PSOG
  {3U, 0U, 1U, 2U}, // GSPO
  {3U, 0U, 2U, 1U}, // GSPO
  {3U, 2U, 1U, 0U}, // GOPS
  {3U, 2U, 0U, 1U}, // GOPS
  {3U, 1U, 0U, 2U}, // GPSO
  {3U, 1U, 2U, 0U}  // GPSO
};

const SerdStatement*
serd_cursor_get_internal(const SerdCursor* cursor);

SerdCursor
serd_cursor_make(const SerdModel*      model,
                 ZixBTreeIter          iter,
                 const SerdNode* const pattern[4],
                 ScanStrategy          strategy);

SerdStatus
serd_cursor_scan_next(SerdCursor* cursor);

bool
serd_iter_in_range(ZixBTreeIter          iter,
                   const SerdNode* const pattern[4],
                   ScanStrategy          strategy);

#endif // SERD_SRC_CURSOR_H
