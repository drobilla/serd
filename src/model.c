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

#include "model.h"

#include "compare.h"
#include "cursor.h"
#include "iter.h"
#include "node.h"
#include "range.h"
#include "statement.h"
#include "world.h"

#include "zix/btree.h"
#include "zix/common.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define DEFAULT_ORDER SERD_ORDER_SPO
#define DEFAULT_GRAPH_ORDER SERD_ORDER_GSPO

static const SerdQuad wildcard_pattern = {0, 0, 0, 0};

typedef struct {
  SearchMode         mode;
  unsigned           n_prefix;
  unsigned           n_orders;
  SerdStatementOrder orders[2];
} SerdIterLens;

static const SerdIterLens perfect_lenses[] = {
  [0x0000] = {ALL, 0u, 2u, {SERD_ORDER_GSPO, SERD_ORDER_SPO}},
  [0x0001] = {RANGE, 0u, 1u, {SERD_ORDER_GSPO}},

  [0x0010] = {RANGE, 1u, 2u, {SERD_ORDER_OPS, SERD_ORDER_OSP}},
  [0x0011] = {RANGE, 2u, 2u, {SERD_ORDER_GOPS, SERD_ORDER_GOSP}},

  [0x0100] = {RANGE, 1u, 2u, {SERD_ORDER_POS, SERD_ORDER_PSO}},
  [0x0101] = {RANGE, 2u, 2u, {SERD_ORDER_GPOS, SERD_ORDER_GPSO}},

  [0x0110] = {RANGE, 2u, 2u, {SERD_ORDER_OPS, SERD_ORDER_POS}},
  [0x0111] = {RANGE, 3u, 2u, {SERD_ORDER_GOPS, SERD_ORDER_GPOS}},

  [0x1000] = {RANGE, 1u, 2u, {SERD_ORDER_SPO, SERD_ORDER_SOP}},
  [0x1001] = {RANGE, 2u, 2u, {SERD_ORDER_GSPO, SERD_ORDER_GSOP}},

  [0x1010] = {RANGE, 2u, 2u, {SERD_ORDER_SOP, SERD_ORDER_OSP}},
  [0x1011] = {RANGE, 3u, 2u, {SERD_ORDER_GSOP, SERD_ORDER_GOSP}},

  [0x1100] = {RANGE, 2u, 2u, {SERD_ORDER_SPO, SERD_ORDER_PSO}},
  [0x1101] = {RANGE, 3u, 2u, {SERD_ORDER_GSPO, SERD_ORDER_GPSO}},

  [0x1110] = {RANGE, 3u, 2u, {SERD_ORDER_SPO, SERD_ORDER_OPS}},
  [0x1111] = {RANGE, 4u, 2u, {SERD_ORDER_GSPO, SERD_ORDER_GOPS}},
};

static const SerdIterLens partial_lenses[] = {
  [0x0000] = {ALL, 0u, 0u, {}},
  [0x0001] = {ALL, 0u, 0u, {}},

  [0x0010] = {ALL, 0u, 0u, {}},
  [0x0011] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_GSPO, SERD_ORDER_GPOS}},

  [0x0100] = {ALL, 0u, 0u, {}},
  [0x0101] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_GSPO, SERD_ORDER_GOPS}},

  [0x0110] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_OSP, SERD_ORDER_PSO}},
  [0x0111] = {FILTER_RANGE, 2u, 2u, {SERD_ORDER_GOSP, SERD_ORDER_GPSO}},

  [0x1000] = {ALL, 0u, 0u, {}},
  [0x1001] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_GOPS, SERD_ORDER_GPSO}},

  [0x1010] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_SPO, SERD_ORDER_OPS}},
  [0x1011] = {FILTER_RANGE, 2u, 2u, {SERD_ORDER_GSPO, SERD_ORDER_GOPS}},

  [0x1100] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_SOP, SERD_ORDER_POS}},
  [0x1101] = {FILTER_RANGE, 2u, 2u, {SERD_ORDER_GSOP, SERD_ORDER_GPOS}},

  [0x1110] = {ALL, 0u, 0u, {}},
  [0x1111] = {ALL, 0u, 0u, {}},
};

static ZixBTree*
serd_model_default_index(SerdModel* const model)
{
  return model->indices[SERD_ORDER_GSPO] ? model->indices[SERD_ORDER_GSPO]
                                         : model->indices[SERD_ORDER_SPO];
}

static ZixComparator
serd_model_comparator(const SerdModel* const   model,
                      const SerdStatementOrder order)
{
  (void)model;

  return serd_quad_compare;
  return (ZixComparator)(order < SERD_ORDER_GSPO ? serd_triple_compare
                                                 : serd_quad_compare);
}

static SerdStatus
serd_model_add_index(SerdModel* const model, const SerdStatementOrder order)
{
  if (model->indices[order]) {
    return SERD_FAILURE;
  }

  //  ZixBTree* const     index      = model->indices[order];
  const int* const    ordering   = orderings[order];
  const ZixComparator comparator = serd_model_comparator(model, order);

  if (!(model->indices[order] = zix_btree_new(comparator, ordering))) {
    return SERD_ERR_INTERNAL;
  }

  ZixBTree* const default_index = serd_model_default_index(model);

  if (false) { // TODO
    for (ZixBTreeIter i = zix_btree_begin(default_index);
         !zix_btree_iter_is_end(i);
         zix_btree_iter_increment(&i)) {
      zix_btree_insert(model->indices[order], zix_btree_get(i));
    }
  }

  return SERD_SUCCESS;
}

static SerdStatus
serd_model_drop_index(SerdModel* const model, const SerdStatementOrder order)
{
  if (!model->indices[order]) {
    return SERD_FAILURE;
  }

  if (model->indices[order] == serd_model_default_index(model)) {
    return SERD_ERR_BAD_CALL;
  }

  zix_btree_free(model->indices[order], NULL);
  model->indices[order] = NULL;
  return SERD_SUCCESS;
}

SerdModel*
serd_model_new(SerdWorld* const world, const SerdModelFlags flags)
{
  SerdModel* model = (SerdModel*)calloc(1, sizeof(struct SerdModelImpl));

  model->world = world;
  model->nodes = serd_nodes_new();
  model->flags = flags | SERD_INDEX_SPO; // SPO index is mandatory

  for (unsigned i = 0; i < (NUM_ORDERS / 2); ++i) {
    const SerdStatementOrder order = (SerdStatementOrder)i;

    if (model->flags & (1u << i)) {
      const int* const    ordering   = orderings[i];
      const int* const    g_ordering = orderings[i + (NUM_ORDERS / 2)];
      const ZixComparator comparator = serd_model_comparator(model, order);

      model->indices[i] = zix_btree_new(comparator, ordering);

      if (model->flags & SERD_INDEX_GRAPHS) {
        model->indices[i + (NUM_ORDERS / 2)] = zix_btree_new(
          (ZixComparator)serd_quad_compare, (const void*)g_ordering);
      }
    }
  }

  // Create end iterator
  const SerdStatementOrder order =
    model->indices[SERD_ORDER_GSPO] ? SERD_ORDER_GSPO : SERD_ORDER_SPO;

  ZixBTreeIter   cur = zix_btree_end(model->indices[order]);
  const SerdQuad pat = {0, 0, 0, 0};

  model->end = serd_iter_new(model, cur, pat, order, ALL, 0);

  return model;
}

SerdModel*
serd_model_copy(const SerdModel* const model)
{
  if (!model) {
    return NULL;
  }

  SerdModel* copy = serd_model_new(model->world, model->flags);

  SerdRange* all = serd_model_all(model);
  serd_model_add_range(copy, all);
  serd_range_free(all);

  assert(serd_model_size(model) == serd_model_size(copy));
  assert(serd_nodes_size(model->nodes) == serd_nodes_size(model->nodes));
  return copy;
}

SERD_API
bool
serd_model_equals(const SerdModel* const a, const SerdModel* const b)
{
  if (!a && !b) {
    return true;
  }

  if (!a || !b || serd_model_size(a) != serd_model_size(b)) {
    return false;
  }

  SerdRange* ra = serd_model_all(a);
  SerdRange* rb = serd_model_all(b);

  bool result = true;
  while (!serd_range_empty(ra) && !serd_range_empty(rb)) {
    if (!serd_statement_equals(serd_range_front(ra), serd_range_front(rb))) {
      result = false;
      break;
    }

    serd_range_next(ra);
    serd_range_next(rb);
  }

  result = result && serd_range_empty(ra) && serd_range_empty(rb);

  serd_range_free(ra);
  serd_range_free(rb);
  return result;
}

static void
serd_model_drop_statement(SerdModel* const     model,
                          SerdStatement* const statement)
{
  assert(statement);

  for (unsigned i = 0; i < TUP_LEN; ++i) {
    if (statement->nodes[i]) {
      serd_nodes_deref(model->nodes, statement->nodes[i]);
    }
  }

  if (statement->cursor && statement->cursor->file) {
    serd_nodes_deref(model->nodes, statement->cursor->file);
  }

  serd_statement_free(statement);
}

void
serd_model_free(SerdModel* const model)
{
  if (!model) {
    return;
  }

  serd_iter_free(model->end);

  ZixBTree* const main_index =
    model->indices[model->indices[DEFAULT_GRAPH_ORDER] ? DEFAULT_GRAPH_ORDER
                                                       : DEFAULT_ORDER];

  // Free statements from main index
  zix_btree_clear(main_index, (ZixDestroyFunc)serd_statement_free);

#if 0
  ZixBTreeIter t = zix_btree_begin(index);
  for (; !zix_btree_iter_is_end(t); zix_btree_iter_increment(&t)) {
    serd_statement_free((SerdStatement*)zix_btree_get(t));
  }
#endif

  // Free indices
  for (unsigned o = 0; o < NUM_ORDERS; ++o) {
    if (model->indices[o]) {
      zix_btree_free(model->indices[o], NULL);
    }
  }

  serd_nodes_free(model->nodes);
  free(model);
}

SerdWorld*
serd_model_world(SerdModel* const model)
{
  return model->world;
}

const SerdNodes*
serd_model_nodes(const SerdModel* const model)
{
  return model->nodes;
}

SerdModelFlags
serd_model_flags(const SerdModel* const model)
{
  return model->flags;
}

size_t
serd_model_size(const SerdModel* const model)
{
  const SerdStatementOrder order =
    model->indices[SERD_ORDER_GSPO] ? SERD_ORDER_GSPO : SERD_ORDER_SPO;
  return zix_btree_size(model->indices[order]);
}

bool
serd_model_empty(const SerdModel* const model)
{
  return serd_model_size(model) == 0;
}

// FIXME : expose

static SerdIter*
serd_model_begin_ordered(const SerdModel* const   model,
                         const SerdStatementOrder order)
{
  return model->indices[order]
           ? serd_iter_new(model,
                           zix_btree_begin(model->indices[order]),
                           wildcard_pattern,
                           order,
                           ALL,
                           0)
           : NULL;
}

static SerdIter*
serd_model_end_ordered(const SerdModel* const   model,
                       const SerdStatementOrder order)
{
  return model->indices[order]
           ? serd_iter_new(model,
                           zix_btree_end(model->indices[order]),
                           wildcard_pattern,
                           order,
                           ALL,
                           0)
           : NULL;
}

SerdIter*
serd_model_begin(const SerdModel* const model)
{
  if (serd_model_empty(model)) {
    return serd_iter_copy(serd_model_end(model));
  }

  const SerdStatementOrder order =
    model->indices[SERD_ORDER_GSPO] ? SERD_ORDER_GSPO : SERD_ORDER_SPO;

  ZixBTreeIter   cur = zix_btree_begin(model->indices[order]);
  const SerdQuad pat = {0, 0, 0, 0};
  return serd_iter_new(model, cur, pat, order, ALL, 0);
}

const SerdIter*
serd_model_end(const SerdModel* const model)
{
  return model->end;
}

SerdRange*
serd_model_all(const SerdModel* const model)
{
  return serd_range_new(serd_model_begin(model),
                        serd_iter_copy(serd_model_end(model)));
}

SerdRange*
serd_model_ordered(const SerdModel* const model, const SerdStatementOrder order)
{
  const SerdStatementOrder real_order =
    (order >= SERD_ORDER_GSPO && !(model->flags & SERD_INDEX_GRAPHS))
      ? (SerdStatementOrder)(order - SERD_ORDER_GSPO)
      : order;

  if (!model->indices[real_order]) {
    return NULL;
  }

  return serd_range_new(serd_model_begin_ordered(model, real_order),
                        serd_model_end_ordered(model, real_order));
}

static bool
serd_model_supports_lens(const SerdModel* const    model,
                         const SerdIterLens        lens,
                         SerdStatementOrder* const order)
{
  for (unsigned i = 0u; i < lens.n_orders; ++i) {
    if (model->indices[lens.orders[i]]) {
      *order = lens.orders[i];
      return true;
    }
  }

  return false;
}

SerdIter*
serd_model_find(const SerdModel* const model,
                const SerdNode* const  s,
                const SerdNode* const  p,
                const SerdNode* const  o,
                const SerdNode* const  g)
{
  // Build a 4-bit signature for this pattern: SPOG, set if a node is given
  const SerdQuad pat = {s, p, o, g};
  const unsigned sig = ((pat[0] ? 1u : 0u) * 0x1000 + //
                        (pat[1] ? 1u : 0u) * 0x0100 + //
                        (pat[2] ? 1u : 0u) * 0x0010 + //
                        (pat[3] ? 1u : 0u) * 0x0001);

  // Grab perfect and partial lenses to try from the tables
  const SerdIterLens perfect = perfect_lenses[sig];
  const SerdIterLens partial = partial_lenses[sig];

  if (sig == 0x0000) {
    return serd_model_begin(model);
  }

  SerdStatementOrder index_order = SERD_ORDER_SPO;
  SerdIterLens       lens        = {FILTER_ALL, 0u, 1u, {index_order}};

  if (serd_model_supports_lens(model, perfect, &index_order)) {
    lens = perfect;
  } else if (serd_model_supports_lens(model, partial, &index_order)) {
    lens = partial;
  } else if (pat[3]) {
    lens.mode   = FILTER_RANGE;
    index_order = SERD_ORDER_GSPO;
  }

  ZixBTree* const index = model->indices[index_order];
  ZixBTreeIter    cur   = zix_btree_end(index);

  assert(index); // FIXME?

  if (lens.mode == ALL || lens.mode == FILTER_ALL) {
    // No prefix shared with an index at all, linear search (worst case)
    cur = zix_btree_begin(index);
  } else if (lens.mode == FILTER_RANGE) {
    /* Some prefix, but filtering still required.  Build a search pattern
       with only the prefix to find the lower bound in log time. */
    SerdQuad         prefix_pat = {NULL, NULL, NULL, NULL};
    const int* const ordering   = orderings[index_order];
    for (unsigned i = 0u; i < lens.n_prefix; ++i) {
      prefix_pat[ordering[i]] = pat[ordering[i]];
    }

    zix_btree_lower_bound(index,
                          index_order < SERD_ORDER_GSPO
                            ? (ZixComparator)serd_triple_compare_pattern
                            : (ZixComparator)serd_quad_compare_pattern,
                          ordering,
                          prefix_pat,
                          &cur);

  } else {
    // Ideal case, pattern matches an index with no filtering required
    zix_btree_lower_bound(index,
                          index_order < SERD_ORDER_GSPO
                            ? (ZixComparator)serd_triple_compare_pattern
                            : (ZixComparator)serd_quad_compare_pattern,
                          orderings[index_order],
                          pat,
                          &cur);
  }

  if (zix_btree_iter_is_end(cur)) {
    return NULL;
  }

  const SerdStatement* const key = (const SerdStatement*)zix_btree_get(cur);
  if (!key || (lens.mode == RANGE && !serd_statement_matches_quad(key, pat))) {
    return NULL;
  }

  return serd_iter_new(model, cur, pat, index_order, lens.mode, lens.n_prefix);
}

SerdRange*
serd_model_range(const SerdModel* const model,
                 const SerdNode* const  s,
                 const SerdNode* const  p,
                 const SerdNode* const  o,
                 const SerdNode* const  g)
{
  if (!s && !p && !o && !g) {
    return serd_range_new(serd_model_begin(model),
                          serd_iter_copy(serd_model_end(model)));
  }

  ZixBTreeIter end_cur = {0}; // FIXME

  SerdIter* begin = serd_model_find(model, s, p, o, g);

  SerdIter* end = begin ? serd_iter_new(model,
                                        end_cur,
                                        begin->pat,
                                        begin->order,
                                        begin->mode,
                                        begin->n_prefix)
                        : NULL;

  return serd_range_new(begin, end);
}

const SerdNode*
serd_model_get(const SerdModel* const model,
               const SerdNode* const  s,
               const SerdNode* const  p,
               const SerdNode* const  o,
               const SerdNode* const  g)
{
  const SerdStatement* statement = serd_model_get_statement(model, s, p, o, g);

  if (statement) {
    if (!s) {
      return serd_statement_subject(statement);
    }

    if (!p) {
      return serd_statement_predicate(statement);
    }

    if (!o) {
      return serd_statement_object(statement);
    }

    if (!g) {
      return serd_statement_graph(statement);
    }
  }

  return NULL;
}

const SerdStatement*
serd_model_get_statement(const SerdModel* const model,
                         const SerdNode* const  s,
                         const SerdNode* const  p,
                         const SerdNode* const  o,
                         const SerdNode* const  g)
{
  if ((bool)s + (bool)p + (bool)o != 2 &&
      (bool)s + (bool)p + (bool)o + (bool)g != 3) {
    return NULL;
  }

  SerdIter* const i = serd_model_find(model, s, p, o, g);
  if (i && !zix_btree_iter_is_end(i->cur)) {
    const SerdStatement* statement = serd_iter_get(i);
    serd_iter_free(i);
    return statement;
  }

  return NULL;
}

size_t
serd_model_count(const SerdModel* const model,
                 const SerdNode* const  s,
                 const SerdNode* const  p,
                 const SerdNode* const  o,
                 const SerdNode* const  g)
{
  SerdRange* const range = serd_model_range(model, s, p, o, g);
  size_t           count = 0;

  for (; !serd_range_empty(range); serd_range_next(range)) {
    ++count;
  }

  serd_range_free(range);
  return count;
}

bool
serd_model_ask(const SerdModel* const model,
               const SerdNode* const  s,
               const SerdNode* const  p,
               const SerdNode* const  o,
               const SerdNode* const  g)
{
  SerdIter*  iter = serd_model_find(model, s, p, o, g);
  const bool ret  = iter && !zix_btree_iter_is_end(iter->cur);
  serd_iter_free(iter);
  return ret;
}

static SerdCursor*
serd_model_intern_cursor(SerdModel* const model, const SerdCursor* const cursor)
{
  if (cursor) {
    SerdCursor* copy = (SerdCursor*)calloc(1, sizeof(SerdCursor));

    copy->file = serd_nodes_intern(model->nodes, cursor->file);
    copy->line = cursor->line;
    copy->col  = cursor->col;
    return copy;
  }

  return NULL;
}

SerdStatus
serd_model_add_internal(SerdModel* const        model,
                        const SerdCursor* const cursor,
                        const SerdNode* const   s,
                        const SerdNode* const   p,
                        const SerdNode* const   o,
                        const SerdNode* const   g)
{
  SerdStatement* statement = (SerdStatement*)calloc(1, sizeof(SerdStatement));

  assert(s);
  assert(p);
  assert(o);

  statement->nodes[0] = s;
  statement->nodes[1] = p;
  statement->nodes[2] = o;
  statement->nodes[3] = g;
  statement->cursor   = serd_model_intern_cursor(model, cursor);

  bool added = false;
  for (unsigned i = 0; i < NUM_ORDERS; ++i) {
    if (model->indices[i]) {
      if (!zix_btree_insert(model->indices[i], statement)) {
        added = true;
      } else if (i == SERD_ORDER_GSPO) {
        break; // Statement already indexed
      }
    }
  }

  ++model->version;
  if (added) {
    return SERD_SUCCESS;
  }

  serd_model_drop_statement(model, statement);
  return SERD_FAILURE;
}

SerdStatus
serd_model_add(SerdModel* const      model,
               const SerdNode* const s,
               const SerdNode* const p,
               const SerdNode* const o,
               const SerdNode* const g)
{
  if (!s || !p || !o) {
    return SERD_LOG_ERROR(model->world,
                          SERD_ERR_BAD_ARG,
                          "attempt to add statement with NULL field");
  }

  return serd_model_add_internal(model,
                                 NULL,
                                 serd_nodes_intern(model->nodes, s),
                                 serd_nodes_intern(model->nodes, p),
                                 serd_nodes_intern(model->nodes, o),
                                 serd_nodes_intern(model->nodes, g));
}

SerdStatus
serd_model_insert(SerdModel* const model, const SerdStatement* const statement)
{
  if (!statement) {
    return SERD_FAILURE;
  }

  SerdNodes* nodes = model->nodes;
  return serd_model_add_internal(
    model,
    serd_statement_cursor(statement),
    serd_nodes_intern(nodes, serd_statement_subject(statement)),
    serd_nodes_intern(nodes, serd_statement_predicate(statement)),
    serd_nodes_intern(nodes, serd_statement_object(statement)),
    serd_nodes_intern(nodes, serd_statement_graph(statement)));
}

SerdStatus
serd_model_add_range(SerdModel* const model, SerdRange* const range)
{
  SerdStatus st = SERD_SUCCESS;
  for (; !st && !serd_range_empty(range); serd_range_next(range)) {
    st =
      serd_model_insert(model, (const SerdStatement*)serd_range_front(range));
  }

  return st;
}

SerdStatus
serd_model_erase(SerdModel* const model, SerdIter* const iter)
{
  const SerdStatement* statement = serd_iter_get(iter);
  SerdStatement*       removed   = NULL;
  ZixStatus            zst       = ZIX_STATUS_SUCCESS;

  assert(statement);

  // Erase from the index associated with this iterator
  zst = zix_btree_remove(
    model->indices[iter->order], statement, (void**)&removed, &iter->cur);

  if (zst == ZIX_STATUS_NOT_FOUND) {
    assert(!removed);
    return SERD_FAILURE;
  }

  if (zst) {
    return SERD_ERR_INTERNAL;
  }

  assert(removed);
  assert(removed == statement);

  for (int i = SERD_ORDER_SPO; i <= SERD_ORDER_GPOS; ++i) {
    if (model->indices[i] && i != (int)iter->order) {
      SerdStatement* index_removed = NULL;
      ZixBTreeIter   next          = {0}; // FIXME

      zst = zix_btree_remove(
        model->indices[i], statement, (void**)&index_removed, &next);

      if (zst && zst != ZIX_STATUS_NOT_FOUND) {
        return SERD_ERR_INTERNAL;
      }

      // FIXME removing triples from graph indices does this
      //      assert(!index_removed || index_removed == removed);
    }
  }
  serd_iter_scan_next(iter);

  serd_model_drop_statement(model, removed);
  iter->version = ++model->version;

  return SERD_SUCCESS;
}

SerdStatus
serd_model_erase_range(SerdModel* const model, SerdRange* const range)
{
  while (!serd_range_empty(range)) {
    serd_model_erase(model, range->begin);
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_model_clear(SerdModel* const model)
{
  SerdIter*             i   = serd_model_begin(model);
  const SerdIter* const end = serd_model_end(model);

  while (!serd_iter_equals(i, end)) {
    serd_model_erase(model, i);
  }

  serd_iter_free(i);
  return SERD_SUCCESS;
}
