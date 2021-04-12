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

#include "iter.h"

#include "model.h"
#include "node.h"
#include "world.h"

#include "serd/serd.h"
#include "zix/btree.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool
serd_iter_pattern_matches(const SerdIter* const iter, const SerdQuad pat)
{
  const SerdStatement* key = (const SerdStatement*)zix_btree_get(iter->cur);
  for (int i = 0; i < iter->n_prefix; ++i) {
    const int idx = orderings[iter->order][i];
    if (!serd_node_pattern_match(key->nodes[idx], pat[idx])) {
      return false;
    }
  }

  return true;
}

/**
   Seek forward as necessary until `iter` points at a matching statement.

   @return true iff iterator reached end of valid range.
*/
static bool
serd_iter_seek_match(SerdIter* const iter, const SerdQuad pat)
{
  for (; !zix_btree_iter_is_end(iter->cur);
       zix_btree_iter_increment(iter->cur)) {
    const SerdStatement* s = (const SerdStatement*)zix_btree_get(iter->cur);
    if (serd_statement_matches_quad(s, pat)) {
      return false; // Found match
    }

    if (iter->mode == FILTER_RANGE && !serd_iter_pattern_matches(iter, pat)) {
      zix_btree_iter_free(iter->cur);
      iter->cur = NULL;
      return true; // Reached end of range
    }
  }

  assert(zix_btree_iter_is_end(iter->cur));
  return true; // Reached end of index
}

static bool
check_version(const SerdIter* const iter)
{
  if (!iter || iter->version != iter->model->version) {
    SERD_LOG_ERROR(iter->model->world,
                   SERD_ERR_BAD_ITER,
                   "attempt to use invalidated iterator");
    return false;
  }
  return true;
}

SerdIter*
serd_iter_new(const SerdModel* const   model,
              ZixBTreeIter* const      cur,
              const SerdQuad           pat,
              const SerdStatementOrder order,
              const SearchMode         mode,
              const int                n_prefix)
{
  SerdIter* const iter = (SerdIter* const)calloc(1, sizeof(SerdIter));
  iter->model          = model;
  iter->cur            = cur;
  iter->order          = order;
  iter->mode           = mode;
  iter->n_prefix       = n_prefix;
  iter->version        = model->version;

  switch (iter->mode) {
  case ALL:
  case RANGE:
    assert(zix_btree_iter_is_end(iter->cur) ||
           serd_statement_matches_quad(
             ((const SerdStatement*)zix_btree_get(iter->cur)), pat));
    break;
  case FILTER_RANGE:
  case FILTER_ALL:
    serd_iter_seek_match(iter, pat);
    break;
  }

  // Replace (possibly temporary) nodes in pattern with nodes from the model
  if (!zix_btree_iter_is_end(iter->cur)) {
    const SerdStatement* s = (const SerdStatement*)zix_btree_get(iter->cur);
    for (int i = 0; i < TUP_LEN; ++i) {
      if (pat[i]) {
        iter->pat[i] = s->nodes[i];
      }
    }
  }

  return iter;
}

SerdIter*
serd_iter_copy(const SerdIter* const iter)
{
  if (!iter) {
    return NULL;
  }

  SerdIter* const copy = (SerdIter* const)malloc(sizeof(SerdIter));
  memcpy(copy, iter, sizeof(SerdIter));
  copy->cur = zix_btree_iter_copy(iter->cur);
  return copy;
}

const SerdStatement*
serd_iter_get(const SerdIter* const iter)
{
  return ((check_version(iter) && !zix_btree_iter_is_end(iter->cur))
            ? (const SerdStatement*)zix_btree_get(iter->cur)
            : NULL);
}

bool
serd_iter_scan_next(SerdIter* const iter)
{
  if (zix_btree_iter_is_end(iter->cur)) {
    return true;
  }

  bool is_end = false;
  switch (iter->mode) {
  case ALL:
    // At the end if the cursor is (assigned above)
    break;
  case RANGE:
    // At the end if the MSNs no longer match
    if (!serd_iter_pattern_matches(iter, iter->pat)) {
      zix_btree_iter_free(iter->cur);
      iter->cur = NULL;
      is_end    = true;
    }
    break;
  case FILTER_RANGE:
  case FILTER_ALL:
    // Seek forward to next match
    is_end = serd_iter_seek_match(iter, iter->pat);
    break;
  }

  return is_end;
}

bool
serd_iter_next(SerdIter* const iter)
{
  if (zix_btree_iter_is_end(iter->cur) || !check_version(iter)) {
    return true;
  }

  zix_btree_iter_increment(iter->cur);
  return serd_iter_scan_next(iter);
}

bool
serd_iter_equals(const SerdIter* const lhs, const SerdIter* const rhs)
{
  if (!lhs && !rhs) {
    return true;
  }

  return (lhs && rhs && lhs->model == rhs->model &&
          zix_btree_iter_equals(lhs->cur, rhs->cur) &&
          serd_node_pattern_match(lhs->pat[0], rhs->pat[0]) &&
          serd_node_pattern_match(lhs->pat[1], rhs->pat[1]) &&
          serd_node_pattern_match(lhs->pat[2], rhs->pat[2]) &&
          serd_node_pattern_match(lhs->pat[3], rhs->pat[3]) &&
          lhs->order == rhs->order && lhs->mode == rhs->mode &&
          lhs->n_prefix == rhs->n_prefix);
}

void
serd_iter_free(SerdIter* const iter)
{
  if (iter) {
    zix_btree_iter_free(iter->cur);
    free(iter);
  }
}
