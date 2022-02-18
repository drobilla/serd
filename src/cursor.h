/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SERD_CURSOR_H
#define SERD_CURSOR_H

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
                 const SerdNode*  pattern[4],
                 ScanStrategy     strategy);

SerdStatus
serd_cursor_scan_next(SerdCursor* cursor);

bool
serd_iter_in_range(ZixBTreeIter          iter,
                   const SerdNode* const pattern[4],
                   ScanStrategy          strategy);

#endif // SERD_CURSOR_H
