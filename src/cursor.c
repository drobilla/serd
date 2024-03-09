// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "cursor.h"

#include "log.h"
#include "match.h"
#include "model.h"
#include "statement.h"

#include "serd/log.h"
#include "serd/statement_view.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/btree.h"
#include "zix/status.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static inline bool
statement_matches_quad(const SerdStatement* const statement,
                       const SerdNode* const      quad[4])
{
  return serd_statement_matches(statement, quad[0], quad[1], quad[2], quad[3]);
}

ZIX_PURE_FUNC bool
serd_iter_in_range(const ZixBTreeIter    iter,
                   const SerdNode* const pattern[4],
                   const ScanStrategy    strategy)
{
  const SerdStatement* const statement =
    (const SerdStatement*)zix_btree_get(iter);

  for (unsigned i = 0U; i < strategy.n_prefix; ++i) {
    const unsigned field = orderings[strategy.order][i];
    if (!serd_match_node(statement->nodes[field], pattern[field])) {
      return false;
    }
  }

  return true;
}

static bool
serd_cursor_in_range(const SerdCursor* const cursor)
{
  return (cursor->strategy.mode == FILTER_EVERYTHING ||
          serd_iter_in_range(cursor->iter, cursor->pattern, cursor->strategy));
}

/// Seek forward as necessary until the cursor points to a matching statement
static SerdStatus
serd_cursor_seek_match(SerdCursor* const cursor)
{
  assert(cursor->strategy.mode == FILTER_EVERYTHING ||
         cursor->strategy.mode == FILTER_RANGE);

  for (; !zix_btree_iter_is_end(cursor->iter);
       zix_btree_iter_increment(&cursor->iter)) {
    if (!serd_cursor_in_range(cursor)) {
      // Went past the end of the matching range, reset to end
      cursor->iter = zix_btree_end_iter;
      return SERD_FAILURE;
    }

    const SerdStatement* s = (const SerdStatement*)zix_btree_get(cursor->iter);
    if (statement_matches_quad(s, cursor->pattern)) {
      break; // Found matching statement
    }
  }

  return SERD_SUCCESS;
}

static bool
check_version(const SerdCursor* const cursor)
{
  if (cursor->version != cursor->model->version) {
    serd_logf(cursor->model->world,
              SERD_LOG_LEVEL_ERROR,
              "attempt to use invalidated cursor");
    return false;
  }

  return true;
}

SerdCursor
serd_cursor_make(const SerdModel* const model,
                 const ZixBTreeIter     iter,
                 const SerdNode* const  pattern[4],
                 const ScanStrategy     strategy)
{
  SerdCursor cursor = {model,
                       {pattern[0], pattern[1], pattern[2], pattern[3]},
                       model->version,
                       iter,
                       strategy};

  if (strategy.mode == FILTER_RANGE || strategy.mode == FILTER_EVERYTHING) {
    serd_cursor_seek_match(&cursor);
  }

#ifndef NDEBUG
  if (!zix_btree_iter_is_end(cursor.iter)) {
    // Check that the cursor is at a matching statement
    const SerdStatement* first =
      (const SerdStatement*)zix_btree_get(cursor.iter);
    assert(statement_matches_quad(first, pattern));

    // Check that any nodes in the pattern are interned
    for (unsigned i = 0U; i < 3; ++i) {
      assert(!cursor.pattern[i] || cursor.pattern[i] == first->nodes[i]);
    }
  }
#endif

  return cursor;
}

SerdCursor*
serd_cursor_copy(ZixAllocator* const allocator, const SerdCursor* const cursor)
{
  if (!cursor) {
    return NULL;
  }

  SerdCursor* const copy =
    (SerdCursor* const)zix_malloc(allocator, sizeof(SerdCursor));

  if (copy) {
    memcpy(copy, cursor, sizeof(SerdCursor));
  }

  return copy;
}

const SerdStatement*
serd_cursor_get_internal(const SerdCursor* const cursor)
{
  return (
    (cursor && !zix_btree_iter_is_end(cursor->iter) && check_version(cursor))
      ? (const SerdStatement*)zix_btree_get(cursor->iter)
      : NULL);
}

SerdStatementView
serd_cursor_get(const SerdCursor* ZIX_NULLABLE cursor)
{
  const SerdStatement* const statement = serd_cursor_get_internal(cursor);
  if (statement) {
    return serd_statement_view(statement);
  }

  const SerdStatementView no_statement = {NULL, NULL, NULL, NULL, {NULL, 0, 0}};
  return no_statement;
}

SerdStatus
serd_cursor_scan_next(SerdCursor* const cursor)
{
  if (zix_btree_iter_is_end(cursor->iter) || !check_version(cursor)) {
    return SERD_BAD_CURSOR;
  }

  switch (cursor->strategy.mode) {
  case SCAN_EVERYTHING:
    break;

  case SCAN_RANGE:
    if (!serd_cursor_in_range(cursor)) {
      // Went past the end of the matching range
      cursor->iter = zix_btree_end_iter;
      return SERD_FAILURE;
    }
    break;

  case FILTER_EVERYTHING:
  case FILTER_RANGE:
    // Seek forward to next match
    return serd_cursor_seek_match(cursor);
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_cursor_advance(SerdCursor* const cursor)
{
  if (!cursor) {
    return SERD_FAILURE;
  }

  if (zix_btree_iter_is_end(cursor->iter) || !check_version(cursor)) {
    return SERD_BAD_CURSOR;
  }

  const ZixStatus zst = zix_btree_iter_increment(&cursor->iter);
  if (zst) {
    assert(zst == ZIX_STATUS_REACHED_END);
    return SERD_FAILURE;
  }

  return serd_cursor_scan_next(cursor);
}

bool
serd_cursor_is_end(const SerdCursor* const cursor)
{
  return !cursor || zix_btree_iter_is_end(cursor->iter);
}

bool
serd_cursor_equals(const SerdCursor* const lhs, const SerdCursor* const rhs)
{
  if (serd_cursor_is_end(lhs) || serd_cursor_is_end(rhs)) {
    return serd_cursor_is_end(lhs) && serd_cursor_is_end(rhs);
  }

  /* We don't bother checking if the patterns match each other here, or if both
     cursors have the same ordering, since both of these must be true if the
     BTree iterators are equal. */

  return (zix_btree_iter_equals(lhs->iter, rhs->iter) &&
          lhs->strategy.mode == rhs->strategy.mode &&
          lhs->strategy.n_prefix == rhs->strategy.n_prefix);
}

void
serd_cursor_free(ZixAllocator* const allocator, SerdCursor* const cursor)
{
  zix_free(allocator, cursor);
}
