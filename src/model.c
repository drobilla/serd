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

#include <stdio.h>

static const SerdQuad wildcard_pattern = {0, 0, 0, 0};

typedef struct {
  SearchMode         mode;
  unsigned           n_prefix;
  unsigned           n_orders;
  SerdStatementOrder orders[2];
} SerdSearchStrategy;

static const char*
serd_model_order_string(const SerdModel* const   model,
                        const SerdStatementOrder order)
{
  static const char* const quad_order_strings[] = {
    [SERD_ORDER_SPO]  = "S P O G",
    [SERD_ORDER_SOP]  = "S O P G",
    [SERD_ORDER_OPS]  = "O P S G",
    [SERD_ORDER_OSP]  = "O S P G",
    [SERD_ORDER_PSO]  = "P S O G",
    [SERD_ORDER_POS]  = "P O S G",
    [SERD_ORDER_GSPO] = "G S P O",
    [SERD_ORDER_GSOP] = "G S O P",
    [SERD_ORDER_GOPS] = "G O P S",
    [SERD_ORDER_GOSP] = "G O S P",
    [SERD_ORDER_GPSO] = "G P S O",
    [SERD_ORDER_GPOS] = "G P O S",
  };

  static const char* const triple_order_strings[] = {
    [SERD_ORDER_SPO] = "S P O",
    [SERD_ORDER_SOP] = "S O P",
    [SERD_ORDER_OPS] = "O P S",
    [SERD_ORDER_OSP] = "O S P",
    [SERD_ORDER_PSO] = "P S O",
    [SERD_ORDER_POS] = "P O S",
  };

  return (order >= SERD_ORDER_GSPO || (model->flags & SERD_STORE_GRAPHS))
           ? quad_order_strings[order]
           : triple_order_strings[order];
}

static ZixComparator
serd_model_index_comparator(const SerdModel* const   model,
                            const SerdStatementOrder order)
{
  return (order < SERD_ORDER_GSPO && !(model->flags & SERD_STORE_GRAPHS))
           ? serd_triple_compare
           : serd_quad_compare;
}

static ZixComparator
serd_model_pattern_comparator(const SerdModel* const   model,
                              const SerdStatementOrder order)
{
  return (order < SERD_ORDER_GSPO && !(model->flags & SERD_STORE_GRAPHS))
           ? serd_triple_compare_pattern
           : serd_quad_compare_pattern;
}

SerdStatus
serd_model_add_index(SerdModel* const model, const SerdStatementOrder order)
{
  if (model->indices[order]) {
    return SERD_FAILURE;
  }

  const int* const    ordering   = orderings[order];
  const ZixComparator comparator = serd_model_index_comparator(model, order);
  if (!(model->indices[order] = zix_btree_new(comparator, ordering))) {
    return SERD_ERR_INTERNAL;
  }

  if (order != model->default_order) {
    ZixBTree* const default_index = model->indices[model->default_order];

    // Insert statements from the default index
    for (ZixBTreeIter i = zix_btree_begin(default_index);
         !zix_btree_iter_is_end(i);
         zix_btree_iter_increment(&i)) {
      zix_btree_insert(model->indices[order], zix_btree_get(i));
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_model_drop_index(SerdModel* const model, const SerdStatementOrder order)
{
  if (!model->indices[order]) {
    return SERD_FAILURE;
  }

  if (order == model->default_order) {
    return SERD_ERR_BAD_CALL;
  }

  zix_btree_free(model->indices[order], NULL);
  model->indices[order] = NULL;
  return SERD_SUCCESS;
}

SerdModel*
serd_model_new(SerdWorld* const         world,
               const SerdStatementOrder default_order,
               const SerdModelFlags     flags)
{
  SerdModel* model = (SerdModel*)calloc(1, sizeof(struct SerdModelImpl));

  model->world         = world;
  model->nodes         = serd_nodes_new();
  model->default_order = default_order;
  model->flags         = flags;

  serd_model_add_index(model, default_order);

  // Create end iterator
  ZixBTreeIter   cur = zix_btree_end(model->indices[default_order]);
  const SerdQuad pat = {0, 0, 0, 0};

  model->end = serd_iter_new(model, cur, pat, default_order, ALL, 0);

  return model;
}

SerdModel*
serd_model_copy(const SerdModel* const model)
{
  if (!model) {
    return NULL;
  }

  SerdModel* copy =
    serd_model_new(model->world, model->default_order, model->flags);

  SerdRange* all = serd_model_all(model);
  serd_model_add_range(copy, all);
  serd_range_free(all);

  assert(serd_model_size(model) == serd_model_size(copy));
  assert(serd_nodes_size(model->nodes) == serd_nodes_size(model->nodes));
  return copy;
}

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

  // Free all statements (which are owned by the default index)
  ZixBTree* const default_index = model->indices[model->default_order];
  zix_btree_clear(default_index, (ZixDestroyFunc)serd_statement_free);

  // Free indices themselves
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
  return zix_btree_size(model->indices[model->default_order]);
}

bool
serd_model_empty(const SerdModel* const model)
{
  return serd_model_size(model) == 0;
}

static void
log_bad_index(const SerdModel* const   model,
              const char* const        description,
              const SerdStatementOrder index_order,
              const bool               s,
              const bool               p,
              const bool               o,
              const bool               g)
{
  serd_world_logf_internal(model->world,
                           SERD_ERR_BAD_INDEX,
                           SERD_LOG_LEVEL_WARNING,
                           NULL,
                           "%s index (%s) for (%s %s %s%s) query",
                           description,
                           serd_model_order_string(model, index_order),
                           s ? "S" : "?",
                           p ? "P" : "?",
                           o ? "O" : "?",
                           (model->flags & SERD_STORE_GRAPHS) ? g ? " G" : " ?"
                                                              : "");
}

// FIXME : expose

static SerdIter*
serd_model_begin_ordered(const SerdModel* const   model,
                         const SerdStatementOrder order)
{
  if (!model->indices[order]) {
    log_bad_index(model, "missing", order, false, false, false, false);
    return NULL;
  }

  return serd_iter_new(model,
                       zix_btree_begin(model->indices[order]),
                       wildcard_pattern,
                       order,
                       ALL,
                       0);
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

  ZixBTreeIter   cur = zix_btree_begin(model->indices[model->default_order]);
  const SerdQuad pat = {0, 0, 0, 0};
  return serd_iter_new(model, cur, pat, model->default_order, ALL, 0);
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
  if (!model->indices[order]) {
    log_bad_index(model, "missing", order, false, false, false, false);
    return NULL;
  }

  return serd_range_new(serd_model_begin_ordered(model, order),
                        serd_model_end_ordered(model, order));
}

static bool
serd_model_adopt_strategy(const SerdModel* const model,
                          const bool             with_graph,
                          SerdSearchStrategy*    strategy)
{
  for (unsigned i = 0u; i < strategy->n_orders; ++i) {
    assert(!with_graph || strategy->orders[i] < SERD_ORDER_GSPO);

    const SerdStatementOrder quad_order =
      with_graph ? (strategy->orders[i] + 6u) : strategy->orders[i];

    if (model->indices[quad_order]) {
      strategy->orders[0] = quad_order;
      return true;
    }
  }

  return false;
}

/// Return the best search strategy, with orders[0] set to an available index
static SerdSearchStrategy
serd_model_strategy(const SerdModel* const model,
                    const SerdNode* const  s,
                    const SerdNode* const  p,
                    const SerdNode* const  o,
                    const SerdNode* const  g)
{
  const SerdSearchStrategy perfect_triple_strategies[] = {
    [0x000] = {ALL, 0u, 0u, {}},
    [0x001] = {RANGE, 1u, 2u, {SERD_ORDER_OPS, SERD_ORDER_OSP}},
    [0x010] = {RANGE, 1u, 2u, {SERD_ORDER_PSO, SERD_ORDER_POS}},
    [0x011] = {RANGE, 2u, 2u, {SERD_ORDER_OPS, SERD_ORDER_POS}},
    [0x100] = {RANGE, 1u, 2u, {SERD_ORDER_SPO, SERD_ORDER_SOP}},
    [0x101] = {RANGE, 2u, 2u, {SERD_ORDER_SOP, SERD_ORDER_OSP}},
    [0x110] = {RANGE, 2u, 2u, {SERD_ORDER_SPO, SERD_ORDER_PSO}},
    [0x111] = {RANGE, 3u, 2u, {model->default_order, SERD_ORDER_SPO}},
  };

  const SerdSearchStrategy partial_triple_strategies[] = {
    [0x000] = {ALL, 0u, 0u, {}},
    [0x001] = {ALL, 0u, 0u, {}},
    [0x010] = {ALL, 0u, 0u, {}},
    [0x011] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_OSP, SERD_ORDER_PSO}},
    [0x100] = {ALL, 0u, 0u, {}},
    [0x101] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_SPO, SERD_ORDER_OPS}},
    [0x110] = {FILTER_RANGE, 1u, 2u, {SERD_ORDER_SOP, SERD_ORDER_POS}},
    [0x111] = {ALL, 0u, 0u, {}},
  };

  // Build a hex signature for this pattern: SPO, 1 if a node is given
  const unsigned sig = ((s ? 1u : 0u) * 0x100 + //
                        (p ? 1u : 0u) * 0x010 + //
                        (o ? 1u : 0u) * 0x001);

  // If this is a total wildcard search, scan the whole default order
  if (sig == 0x000 && !g) {
    SerdSearchStrategy all = {ALL, 0u, 0u, {model->default_order}};
    return all;
  }

  // If this is an exact triple search, just use the default order
  if (sig == 0x111) {
    const unsigned     n_prefix = g ? 4u : 3u;
    SerdSearchStrategy exact    = {RANGE, n_prefix, 1, {model->default_order}};
    return exact;
  }

  // Try to use a perfect strategy
  SerdSearchStrategy perfect = perfect_triple_strategies[sig];
  if (serd_model_adopt_strategy(model, g, &perfect)) {
    return perfect;
  }

  // No perfect index, try to use a partial strategy with filtering
  SerdSearchStrategy partial = partial_triple_strategies[sig];
  if (serd_model_adopt_strategy(model, g, &partial)) {
    return partial;
  }

  // Indices don't help with the triple at all, try to at least find a graph
  if (g) {
    for (unsigned i = SERD_ORDER_GSPO; i <= SERD_ORDER_GPOS; ++i) {
      if (model->indices[i]) {
        const SearchMode         mode     = sig == 0x000 ? RANGE : FILTER_RANGE;
        const SerdSearchStrategy strategy = {mode, 1u, 1u, {i}};

        return strategy;
      }
    }
  }

  // All is lost, regress to linear search
  SerdSearchStrategy linear = {FILTER_ALL, 0u, 1u, {model->default_order}};
  return linear;
}

SerdIter*
serd_model_find(const SerdModel* const model,
                const SerdNode* const  s,
                const SerdNode* const  p,
                const SerdNode* const  o,
                const SerdNode* const  g)
{
  const SerdQuad           pattern  = {s, p, o, g};
  const SerdSearchStrategy strategy = serd_model_strategy(model, s, p, o, g);
  const SerdStatementOrder order    = strategy.orders[0];
  ZixBTree* const          index    = model->indices[order];

  if (strategy.mode == ALL) {
    // Total wildcard query, start at the beginning
    return serd_model_begin(model);
  }

  if (strategy.mode == FILTER_ALL) {
    // Worst case scenario, linear search of a useless index with filtering
    log_bad_index(model, "using linear", order, s, p, o, g);
    return serd_iter_new(
      model, zix_btree_begin(index), pattern, order, FILTER_ALL, 0);
  }

  if (strategy.mode == FILTER_RANGE) {
    // Index can constrain to some prefix, but filtering is still required
    log_bad_index(model, "using prefix", order, s, p, o, g);
  }

  // Working within a range in some index, find the first statement in it
  ZixBTreeIter cur = zix_btree_end_iter;
  zix_btree_lower_bound(index,
                        serd_model_pattern_comparator(model, order),
                        orderings[order],
                        pattern,
                        &cur);

  if (zix_btree_iter_is_end(cur)) {
    return NULL;
  }

  const SerdStatement* const first = (const SerdStatement*)zix_btree_get(cur);
  for (unsigned i = 0u; i < strategy.n_prefix; ++i) {
    const int field = orderings[order][i];
    if (!serd_node_pattern_match(first->nodes[field], pattern[field])) {
      return NULL;
    }
  }

  if (strategy.mode == RANGE && !serd_statement_matches_quad(first, pattern)) {
    return NULL;
  }

  return serd_iter_new(
    model, cur, pattern, order, strategy.mode, strategy.n_prefix);
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
