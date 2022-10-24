// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "cursor_impl.h"
#include "cursor_internal.h"
#include "log_internal.h"
#include "model_impl.h"
#include "model_internal.h"
#include "orderings.h"
#include "statement.h"

#include <serd/attributes.h>
#include <serd/caret_view.h>
#include <serd/cursor.h>
#include <serd/field.h>
#include <serd/log.h>
#include <serd/model.h>
#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <serd/status.h>
#include <serd/struct_literal.h>
#include <serd/tuple.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/btree.h>
#include <zix/status.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Note that all of the matching methods here rely on all the nodes being
   interned in the same model. */

static inline bool
node_matches(const SerdNodeID a, const SerdNodeID b)
{
  return !a || !b || a == b;
}

static inline bool
statement_matches_quad(const SerdStatement* const statement,
                       const SerdNodeID           quad[static 4])
{
  assert(statement);

  for (unsigned i = 0U; i < 4U; ++i) {
    if (!node_matches(statement->nodes[i], quad[i])) {
      return false;
    }
  }

  return true;
}

ZIX_PURE_FUNC bool
serd_iter_in_range(const ZixBTreeIter iter,
                   const SerdNodeID   pattern[static 4],
                   const ScanStrategy strategy)
{
  const SerdStatement* const statement =
    (const SerdStatement*)zix_btree_get(iter);

  for (unsigned i = 0U; i < strategy.n_prefix; ++i) {
    const uint8_t field = orderings[strategy.order][i];
    if (!node_matches(statement->nodes[field], pattern[field])) {
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
  if (cursor && cursor->version != cursor->model->version) {
    serd_logf(cursor->model->world,
              SERD_LOG_LEVEL_ERROR,
              serd_no_caret(),
              "attempt to use invalidated cursor");
    return false;
  }

  return !!cursor;
}

SerdCursor
serd_cursor_make(const SerdModel* const model,
                 const ZixBTreeIter     iter,
                 const SerdNodeID       pattern[static 4],
                 const ScanStrategy     strategy)
{
  SerdCursor cursor = {model,
                       model->version,
                       {pattern[0], pattern[1], pattern[2], pattern[3]},
                       strategy,
                       iter};

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

void
serd_cursor_free(ZixAllocator* const allocator, SerdCursor* const cursor)
{
  zix_free(allocator, cursor);
}

const SerdStatement*
serd_cursor_get_internal(const SerdCursor* const cursor)
{
  return ((check_version(cursor) && !zix_btree_iter_is_end(cursor->iter))
            ? (const SerdStatement*)zix_btree_get(cursor->iter)
            : NULL);
}

SERD_API SerdNodeID
serd_cursor_field(const SerdCursor* ZIX_NULLABLE cursor, const SerdField field)
{
  const SerdStatement* const statement = serd_cursor_get_internal(cursor);

  return statement ? statement->nodes[field] : 0U;
}

SerdTuple
serd_cursor_tuple(const SerdCursor* const cursor)
{
  const SerdStatement* const statement = serd_cursor_get_internal(cursor);
  if (statement) {
    const SerdTuple tuple = {{statement->nodes[0],
                              statement->nodes[1],
                              statement->nodes[2],
                              statement->nodes[3]}};
    return tuple;
  }

  const SerdTuple tuple = {{0U, 0U, 0U, 0U}};
  return tuple;
}

SerdModelCaret
serd_cursor_caret(const SerdCursor* const cursor)
{
  return (cursor && cursor->model)
           ? serd_model_statement_caret(cursor->model,
                                        serd_cursor_get_internal(cursor))
           : SERD_STRUCT_LITERAL(SerdModelCaret, 0U, 0U, 0U);
}

SerdStatus
serd_cursor_scan_next(SerdCursor* const cursor)
{
  if (!check_version(cursor) || zix_btree_iter_is_end(cursor->iter)) {
    return SERD_BAD_CURSOR;
  }

  if (cursor->strategy.mode == SCAN_RANGE) {
    if (!serd_cursor_in_range(cursor)) {
      // Went past the end of the matching range
      cursor->iter = zix_btree_end_iter;
      return SERD_FAILURE;
    }
  } else if (cursor->strategy.mode == FILTER_EVERYTHING ||
             cursor->strategy.mode == FILTER_RANGE) {
    // Seek forward to next match
    return serd_cursor_seek_match(cursor);
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_cursor_advance(SerdCursor* const cursor)
{
  if (!check_version(cursor) || zix_btree_iter_is_end(cursor->iter)) {
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
     tree iterators are equal. */

  return (zix_btree_iter_equals(lhs->iter, rhs->iter) &&
          lhs->strategy.mode == rhs->strategy.mode &&
          lhs->strategy.n_prefix == rhs->strategy.n_prefix);
}
