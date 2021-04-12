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

#ifndef SERD_ITER_H
#define SERD_ITER_H

#include "statement.h"

#include "serd/serd.h"
#include "zix/btree.h"

#include <stdbool.h>
#include <stdint.h>

/** Mode for searching or iteration */
typedef enum {
  ALL,          ///< Iterate over entire store
  RANGE,        ///< Iterate over range with equal prefix
  FILTER_RANGE, ///< Iterate over range with equal prefix, filtering
  FILTER_ALL    ///< Iterate to end of store, filtering
} SearchMode;

struct SerdIterImpl {
  const SerdModel*   model;    ///< Model being iterated over
  uint64_t           version;  ///< Model version when iterator was created
  ZixBTreeIter*      cur;      ///< Current DB cursor
  SerdQuad           pat;      ///< Pattern (in ordering order)
  SerdStatementOrder order;    ///< Store order (which index)
  SearchMode         mode;     ///< Iteration mode
  int                n_prefix; ///< Prefix for RANGE and FILTER_RANGE
};

#define NUM_ORDERS 12
#define TUP_LEN 4

/**
   Quads of indices for each order, from most to least significant
   (array indexed by SordOrder)
*/
static const int orderings[NUM_ORDERS][TUP_LEN] = {
  {0, 1, 2, 3},
  {0, 2, 1, 3}, // SPO, SOP
  {2, 1, 0, 3},
  {2, 0, 1, 3}, // OPS, OSP
  {1, 0, 2, 3},
  {1, 2, 0, 3}, // PSO, POS
  {3, 0, 1, 2},
  {3, 0, 2, 1}, // GSPO, GSOP
  {3, 2, 1, 0},
  {3, 2, 0, 1}, // GOPS, GOSP
  {3, 1, 0, 2},
  {3, 1, 2, 0} // GPSO, GPOS
};

SerdIter*
serd_iter_new(const SerdModel*   model,
              ZixBTreeIter*      cur,
              const SerdQuad     pat,
              SerdStatementOrder order,
              SearchMode         mode,
              int                n_prefix);

bool
serd_iter_scan_next(SerdIter* iter);

bool
serd_quad_match(const SerdQuad x, const SerdQuad y);

#endif // SERD_ITER_H
