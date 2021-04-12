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

/**
   Compare quads lexicographically, ignoring graph.

   NULL IDs (equal to 0) are treated as wildcards, always less than every
   other possible ID, except itself.
*/
static int
serd_triple_compare(const void* const x,
                    const void* const y,
                    void* const       user_data)
{
  const int* const           ordering = (const int*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (int i = 0; i < TUP_LEN; ++i) {
    const int o = ordering[i];
    if (o != SERD_GRAPH) {
      const int c = serd_node_wildcard_compare(s->nodes[o], t->nodes[o]);
      if (c) {
        return c;
      }
    }
  }

  return 0;
}

/**
   Compare quads lexicographically, with exact (non-wildcard) graph matching.
*/
static int
serd_quad_compare(const void* const x,
                  const void* const y,
                  void* const       user_data)
{
  const SerdStatement* const s = (const SerdStatement*)x;
  const SerdStatement* const t = (const SerdStatement*)y;

  // Compare graph without wildcard matching
  const int cmp = serd_node_compare(s->nodes[SERD_GRAPH], t->nodes[SERD_GRAPH]);

  return cmp ? cmp : serd_triple_compare(x, y, user_data);
}

/**
   Return true iff `serd` has an index for `order`.
   If `graphs` is true, `order` will be modified to be the
   corresponding order with a G prepended (so G will be the MSN).
*/
static bool
serd_model_has_index(const SerdModel* const    model,
                     SerdStatementOrder* const order,
                     int* const                n_prefix,
                     const bool                graphs)
{
  if (graphs) {
    *order = (SerdStatementOrder)(*order + SERD_ORDER_GSPO);
    *n_prefix += 1;
  }

  return model->indices[*order];
}

/**
   Return the best available index for a pattern.
   @param pat Pattern in standard (S P O G) order
   @param mode Set to the (best) iteration mode for iterating over results
   @param n_prefix Set to the length of the range prefix
   (for `mode` == RANGE and `mode` == FILTER_RANGE)
*/
static SerdStatementOrder
serd_model_best_index(const SerdModel* const model,
                      const SerdQuad         pat,
                      SearchMode* const      mode,
                      int* const             n_prefix)
{
  const bool graph_search = (pat[SERD_GRAPH] != 0);

  const unsigned sig =
    ((pat[0] ? 1u : 0u) * 0x100 + (pat[1] ? 1u : 0u) * 0x010 +
     (pat[2] ? 1u : 0u) * 0x001);

  SerdStatementOrder good[2] = {(SerdStatementOrder)-1, (SerdStatementOrder)-1};

#define PAT_CASE(sig, m, g0, g1, np) \
  case sig:                          \
    *mode     = m;                   \
    good[0]   = g0;                  \
    good[1]   = g1;                  \
    *n_prefix = np;                  \
    break

  // Good orderings that don't require filtering
  *mode     = RANGE;
  *n_prefix = 0;
  switch (sig) {
    PAT_CASE(0x001, RANGE, SERD_ORDER_OPS, SERD_ORDER_OSP, 1);
    PAT_CASE(0x010, RANGE, SERD_ORDER_POS, SERD_ORDER_PSO, 1);
    PAT_CASE(0x011, RANGE, SERD_ORDER_OPS, SERD_ORDER_POS, 2);
    PAT_CASE(0x100, RANGE, SERD_ORDER_SPO, SERD_ORDER_SOP, 1);
    PAT_CASE(0x101, RANGE, SERD_ORDER_SOP, SERD_ORDER_OSP, 2);
    PAT_CASE(0x110, RANGE, SERD_ORDER_SPO, SERD_ORDER_PSO, 2);
  case 0x111:
    *mode     = RANGE;
    *n_prefix = graph_search ? 4 : 3;
    return graph_search ? DEFAULT_GRAPH_ORDER : DEFAULT_ORDER;
  default:
    assert(sig == 0x000);
    assert(graph_search);
    *mode     = RANGE;
    *n_prefix = 1;
    return DEFAULT_GRAPH_ORDER;
  }

  if (*mode == RANGE) {
    if (serd_model_has_index(model, &good[0], n_prefix, graph_search)) {
      return good[0];
    }

    if (serd_model_has_index(model, &good[1], n_prefix, graph_search)) {
      return good[1];
    }
  }

  // Not so good orderings that require filtering, but can
  // still be constrained to a range
  switch (sig) {
    PAT_CASE(0x011, FILTER_RANGE, SERD_ORDER_OSP, SERD_ORDER_PSO, 1);
    PAT_CASE(0x101, FILTER_RANGE, SERD_ORDER_SPO, SERD_ORDER_OPS, 1);
    // SPO is always present, so 0x110 is never reached here
  default:
    break;
  }

  if (*mode == FILTER_RANGE) {
    if (serd_model_has_index(model, &good[0], n_prefix, graph_search)) {
      return good[0];
    }

    if (serd_model_has_index(model, &good[1], n_prefix, graph_search)) {
      return good[1];
    }
  }

  if (graph_search) {
    *mode     = FILTER_RANGE;
    *n_prefix = 1;
    return DEFAULT_GRAPH_ORDER;
  }

  *mode = FILTER_ALL;
  return DEFAULT_ORDER;
}

SerdModel*
serd_model_new(SerdWorld* const world, const SerdModelFlags flags)
{
  SerdModel* model = (SerdModel*)calloc(1, sizeof(struct SerdModelImpl));

  model->world = world;
  model->nodes = serd_nodes_new();
  model->flags = flags | SERD_INDEX_SPO; // SPO index is mandatory

  for (unsigned i = 0; i < (NUM_ORDERS / 2); ++i) {
    const int* const ordering   = orderings[i];
    const int* const g_ordering = orderings[i + (NUM_ORDERS / 2)];

    if (model->flags & (1u << i)) {
      model->indices[i] = zix_btree_new(
        (ZixComparator)serd_triple_compare, (const void*)ordering, NULL);
      if (model->flags & SERD_INDEX_GRAPHS) {
        model->indices[i + (NUM_ORDERS / 2)] = zix_btree_new(
          (ZixComparator)serd_quad_compare, (const void*)g_ordering, NULL);
      }
    }
  }

  // Create end iterator
  const SerdStatementOrder order =
    model->indices[SERD_ORDER_GSPO] ? SERD_ORDER_GSPO : SERD_ORDER_SPO;

  ZixBTreeIter*  cur = zix_btree_end(model->indices[order]);
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

  ZixBTree* index =
    model->indices[model->indices[DEFAULT_GRAPH_ORDER] ? DEFAULT_GRAPH_ORDER
                                                       : DEFAULT_ORDER];

  // Free statements (which drops all node references)
  ZixBTreeIter* t = zix_btree_begin(index);
  for (; !zix_btree_iter_is_end(t); zix_btree_iter_increment(t)) {
    serd_model_drop_statement(model, (SerdStatement*)zix_btree_get(t));
  }
  zix_btree_iter_free(t);

  // Free indices
  for (unsigned o = 0; o < NUM_ORDERS; ++o) {
    if (model->indices[o]) {
      zix_btree_free(model->indices[o]);
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

SerdNodes*
serd_model_nodes(SerdModel* const model)
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
  ZixBTreeIter*  cur = zix_btree_begin(model->indices[order]);
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
      ? order - SERD_ORDER_GSPO
      : order;

  if (!model->indices[real_order]) {
    return NULL;
  }

  return serd_range_new(serd_model_begin_ordered(model, real_order),
                        serd_model_end_ordered(model, real_order));
}

SerdIter*
serd_model_find(const SerdModel* const model,
                const SerdNode* const  s,
                const SerdNode* const  p,
                const SerdNode* const  o,
                const SerdNode* const  g)
{
  const SerdQuad pat = {s, p, o, g};
  if (!pat[0] && !pat[1] && !pat[2] && !pat[3]) {
    return serd_model_begin(model);
  }

  SearchMode               mode     = ALL;
  int                      n_prefix = 0;
  const SerdStatementOrder index_order =
    serd_model_best_index(model, pat, &mode, &n_prefix);

  ZixBTree* const db  = model->indices[index_order];
  ZixBTreeIter*   cur = NULL;

  if (mode == FILTER_ALL) {
    // No prefix shared with an index at all, linear search (worst case)
    cur = zix_btree_begin(db);
  } else if (mode == FILTER_RANGE) {
    /* Some prefix, but filtering still required.  Build a search pattern
       with only the prefix to find the lower bound in log time. */
    SerdQuad         prefix_pat = {NULL, NULL, NULL, NULL};
    const int* const ordering   = orderings[index_order];
    for (int i = 0; i < n_prefix; ++i) {
      prefix_pat[ordering[i]] = pat[ordering[i]];
    }
    zix_btree_lower_bound(db, prefix_pat, &cur);
  } else {
    // Ideal case, pattern matches an index with no filtering required
    zix_btree_lower_bound(db, pat, &cur);
  }

  if (zix_btree_iter_is_end(cur)) {
    zix_btree_iter_free(cur);
    return NULL;
  }

  const SerdStatement* const key = (const SerdStatement*)zix_btree_get(cur);
  if (!key || (mode == RANGE && !serd_statement_matches_quad(key, pat))) {
    zix_btree_iter_free(cur);
    return NULL;
  }

  return serd_iter_new(model, cur, pat, index_order, mode, n_prefix);
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

  SerdIter* begin = serd_model_find(model, s, p, o, g);
  SerdIter* end =
    begin
      ? serd_iter_new(
          model, NULL, begin->pat, begin->order, begin->mode, begin->n_prefix)
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
  if (i && i->cur) {
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

  SerdStatement* removed = NULL;
  for (int i = SERD_ORDER_SPO; i <= SERD_ORDER_GPOS; ++i) {
    if (model->indices[i]) {
      zix_btree_remove(model->indices[i],
                       statement,
                       (void**)&removed,
                       i == (int)iter->order ? &iter->cur : NULL);
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
