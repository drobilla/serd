// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "model.h"

#include "caret_impl.h"
#include "compare.h"
#include "cursor.h"
#include "log.h"
#include "memory.h"
#include "statement_impl.h"
#include "warnings.h"

#include "serd/caret.h"
#include "serd/caret_view.h"
#include "serd/log.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/btree.h"
#include "zix/status.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static const SerdNode* const everything_pattern[4] = {NULL, NULL, NULL, NULL};

/// A 3-bit signature for a triple pattern used as a table index
typedef enum {
  SERD_SIGNATURE_XXX, // 000 (???)
  SERD_SIGNATURE_XXO, // 001 (??O)
  SERD_SIGNATURE_XPX, // 010 (?P?)
  SERD_SIGNATURE_XPO, // 011 (?PO)
  SERD_SIGNATURE_SXX, // 100 (S??)
  SERD_SIGNATURE_SXO, // 101 (S?O)
  SERD_SIGNATURE_SPX, // 110 (SP?)
  SERD_SIGNATURE_SPO, // 111 (SPO)
} SerdPatternSignature;

static SerdPatternSignature
serd_model_pattern_signature(const bool with_s,
                             const bool with_p,
                             const bool with_o)
{
  return (SerdPatternSignature)(((with_s ? 1U : 0U) << 2U) +
                                ((with_p ? 1U : 0U) << 1U) +
                                ((with_o ? 1U : 0U)));
}

static ZixBTreeCompareFunc
serd_model_index_comparator(const SerdModel* const   model,
                            const SerdStatementOrder order)
{
  return (order < SERD_ORDER_GSPO && !(model->flags & SERD_STORE_GRAPHS))
           ? serd_triple_compare
           : serd_quad_compare;
}

static ZixBTreeCompareFunc
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
  assert(model);

  if (model->indices[order]) {
    return SERD_FAILURE;
  }

  const unsigned* const     ordering = orderings[order];
  const ZixBTreeCompareFunc comparator =
    serd_model_index_comparator(model, order);

  model->indices[order] = zix_btree_new(model->allocator, comparator, ordering);

  if (!model->indices[order]) {
    return SERD_BAD_ALLOC;
  }

  // Insert statements from the default index
  ZixStatus zst = ZIX_STATUS_SUCCESS;
  if (order != model->default_order) {
    ZixBTree* const default_index = model->indices[model->default_order];
    for (ZixBTreeIter i = zix_btree_begin(default_index);
         !zst && !zix_btree_iter_is_end(i);
         zix_btree_iter_increment(&i)) {
      zst = zix_btree_insert(model->indices[order], zix_btree_get(i));
    }
  }

  return zst ? serd_model_drop_index(model, order) : SERD_SUCCESS;
}

SerdStatus
serd_model_drop_index(SerdModel* const model, const SerdStatementOrder order)
{
  assert(model);

  if (!model->indices[order]) {
    return SERD_FAILURE;
  }

  if (order == model->default_order) {
    return SERD_BAD_CALL;
  }

  zix_btree_free(model->indices[order], NULL, NULL);
  model->indices[order] = NULL;
  return SERD_SUCCESS;
}

static SerdModel*
serd_model_new_with_allocator(ZixAllocator* const      allocator,
                              SerdWorld* const         world,
                              const SerdStatementOrder default_order,
                              const SerdModelFlags     flags)
{
  assert(world);

  SerdNodes* const nodes = serd_nodes_new(allocator);
  if (!nodes) {
    return NULL;
  }

  SerdModel* model =
    (SerdModel*)zix_calloc(allocator, 1, sizeof(struct SerdModelImpl));

  if (!model) {
    serd_nodes_free(nodes);
    return NULL;
  }

  model->allocator     = allocator;
  model->world         = world;
  model->nodes         = nodes;
  model->default_order = default_order;
  model->flags         = flags;

  if (serd_model_add_index(model, default_order)) {
    serd_nodes_free(nodes);
    serd_wfree(world, model);
    return NULL;
  }

  const ScanStrategy end_strategy = {SCAN_EVERYTHING, 0U, default_order};

  model->end = serd_cursor_make(model,
                                zix_btree_end(model->indices[default_order]),
                                everything_pattern,
                                end_strategy);

  return model;
}

SerdModel*
serd_model_new(SerdWorld* const         world,
               const SerdStatementOrder default_order,
               const SerdModelFlags     flags)
{
  return serd_model_new_with_allocator(
    serd_world_allocator(world), world, default_order, flags);
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
  static const char* const order_strings[] = {
    "S P O G",
    "S O P G",
    "O P S G",
    "O S P G",
    "P S O G",
    "P O S G",
    "G S P O",
    "G S O P",
    "G O P S",
    "G O S P",
    "G P S O",
    "G P O S",
  };

  serd_logf(model->world,
            SERD_LOG_LEVEL_WARNING,
            "%s index (%s) for (%s %s %s%s) query",
            description,
            order_strings[index_order],
            s ? "S" : "?",
            p ? "P" : "?",
            o ? "O" : "?",
            (model->flags & SERD_STORE_GRAPHS) ? g ? " G" : " ?" : "");
}

static SerdCursor
make_begin_cursor(const SerdModel* const model, const SerdStatementOrder order)
{
  if (!model->indices[order]) {
    log_bad_index(model, "missing", order, false, false, false, false);
    return model->end;
  }

  const ScanStrategy strategy = {SCAN_EVERYTHING, 0U, order};

  return serd_cursor_make(model,
                          zix_btree_begin(model->indices[order]),
                          everything_pattern,
                          strategy);
}

SerdModel*
serd_model_copy(ZixAllocator* const allocator, const SerdModel* const model)
{
  assert(model);

  SerdModel* copy = serd_model_new_with_allocator(
    allocator, model->world, model->default_order, model->flags);

  SerdCursor cursor = make_begin_cursor(model, model->default_order);
  serd_model_insert_statements(copy, &cursor);

  assert(serd_model_size(model) == serd_model_size(copy));
  assert(serd_nodes_size(model->nodes) == serd_nodes_size(copy->nodes));
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

  SerdCursor ia = make_begin_cursor(a, a->default_order);
  SerdCursor ib = make_begin_cursor(b, b->default_order);

  while (!serd_cursor_is_end(&ia) && !serd_cursor_is_end(&ib)) {
    if (!serd_statement_equals(serd_cursor_get_internal(&ia),
                               serd_cursor_get_internal(&ib)) ||
        serd_cursor_advance(&ia) > SERD_FAILURE ||
        serd_cursor_advance(&ib) > SERD_FAILURE) {
      return false;
    }
  }

  return serd_cursor_is_end(&ia) && serd_cursor_is_end(&ib);
}

static void
serd_model_drop_statement(SerdModel* const     model,
                          SerdStatement* const statement)
{
  assert(statement);

  for (unsigned i = 0U; i < 4; ++i) {
    if (statement->nodes[i]) {
      serd_nodes_deref(model->nodes, statement->nodes[i]);
    }
  }

  SERD_DISABLE_NULL_WARNINGS
  if (statement->caret && serd_caret_document(statement->caret)) {
    serd_nodes_deref(model->nodes, serd_caret_document(statement->caret));
  }
  SERD_RESTORE_WARNINGS

  serd_statement_free(model->allocator, statement);
}

typedef struct {
  ZixAllocator* allocator;
} DestroyContext;

static void
destroy_tree_statement(void* ptr, const void* user_data)
{
  const DestroyContext* const ctx = (const DestroyContext*)user_data;

  serd_statement_free(ctx->allocator, (SerdStatement*)ptr);
}

void
serd_model_free(SerdModel* const model)
{
  if (!model) {
    return;
  }

  // Free all statements (which are owned by the default index)
  ZixBTree* const      default_index = model->indices[model->default_order];
  const DestroyContext ctx           = {model->allocator};
  zix_btree_clear(default_index, destroy_tree_statement, &ctx);

  // Free indices themselves
  for (unsigned i = 0U; i < N_STATEMENT_ORDERS; ++i) {
    zix_btree_free(model->indices[i], NULL, NULL);
  }

  serd_nodes_free(model->nodes);
  serd_wfree(model->world, model);
}

SerdWorld*
serd_model_world(SerdModel* const model)
{
  assert(model);
  return model->world;
}

const SerdNodes*
serd_model_nodes(const SerdModel* const model)
{
  assert(model);
  return model->nodes;
}

SerdStatementOrder
serd_model_default_order(const SerdModel* const model)
{
  assert(model);
  return model->default_order;
}

SerdModelFlags
serd_model_flags(const SerdModel* const model)
{
  assert(model);
  return model->flags;
}

size_t
serd_model_size(const SerdModel* const model)
{
  assert(model);
  return zix_btree_size(model->indices[model->default_order]);
}

bool
serd_model_empty(const SerdModel* const model)
{
  assert(model);
  return serd_model_size(model) == 0;
}

SerdCursor*
serd_model_begin_ordered(ZixAllocator* const      allocator,
                         const SerdModel* const   model,
                         const SerdStatementOrder order)
{
  assert(model);

  const SerdCursor cursor = make_begin_cursor(model, order);

  return serd_cursor_copy(allocator, &cursor);
}

SerdCursor*
serd_model_begin(ZixAllocator* const allocator, const SerdModel* const model)
{
  assert(model);
  return serd_model_begin_ordered(allocator, model, model->default_order);
}

const SerdCursor*
serd_model_end(const SerdModel* const model)
{
  assert(model);
  return &model->end;
}

static SerdStatementOrder
simple_order(const SerdStatementOrder order)
{
  return order < SERD_ORDER_GSPO ? order : (SerdStatementOrder)(order - 6U);
}

/// Return the best index scanning strategy for a pattern with given nodes
static ScanStrategy
serd_model_strategy(const SerdModel* const model,
                    const bool             with_s,
                    const bool             with_p,
                    const bool             with_o,
                    const bool             with_g)
{
#define N_STRATEGY_TIERS 4U

  const SerdStatementOrder default_order = simple_order(model->default_order);
  const ScanStrategy       none          = {SCAN_EVERYTHING, 0U, default_order};

  const ScanStrategy strategies[N_STRATEGY_TIERS][8] = {
    // Preferred perfect strategies: SPO, SOP, OPS, PSO
    {
      none,
      {SCAN_RANGE, 1U, SERD_ORDER_OPS}, // ??O
      {SCAN_RANGE, 1U, SERD_ORDER_PSO}, // ?P?
      {SCAN_RANGE, 2U, SERD_ORDER_OPS}, // ?PO
      {SCAN_RANGE, 1U, SERD_ORDER_SPO}, // S??
      {SCAN_RANGE, 2U, SERD_ORDER_SOP}, // S?O
      {SCAN_RANGE, 2U, SERD_ORDER_SPO}, // SP?
      {SCAN_RANGE, 3U, default_order}   // SPO
    },

    // Alternate perfect strategies: adds OSP and POS
    {
      none,                             // ???
      {SCAN_RANGE, 1U, SERD_ORDER_OSP}, // ??O
      {SCAN_RANGE, 1U, SERD_ORDER_POS}, // ?P?
      {SCAN_RANGE, 2U, SERD_ORDER_POS}, // ?PO
      {SCAN_RANGE, 1U, SERD_ORDER_SOP}, // S??
      {SCAN_RANGE, 2U, SERD_ORDER_OSP}, // S?O
      {SCAN_RANGE, 2U, SERD_ORDER_PSO}, // SP?
      none                              // SPO
    },

    // Preferred partial prefix strategies
    {
      none,                               // ???
      none,                               // ??O
      none,                               // ?P?
      {FILTER_RANGE, 1U, SERD_ORDER_PSO}, // ?PO
      none,                               // S??
      {FILTER_RANGE, 1U, SERD_ORDER_SPO}, // S?O
      {FILTER_RANGE, 1U, SERD_ORDER_SOP}, // SP?
      none                                // SPO
    },

    // Alternate partial prefix strategies
    {
      none,                               // ???
      none,                               // ??O
      none,                               // ?P?
      {FILTER_RANGE, 1U, SERD_ORDER_OSP}, // ?PO
      none,                               // S??
      {FILTER_RANGE, 1U, SERD_ORDER_OPS}, // S?O
      {FILTER_RANGE, 1U, SERD_ORDER_POS}, // SP?
      none                                // SPO
    },
  };

  // Construct a 3-bit signature for this triple pattern
  const SerdPatternSignature sig =
    serd_model_pattern_signature(with_s, with_p, with_o);
  if (!sig && !with_g) {
    return none;
  }

  // Try to find a strategy we can support, from most to least preferred
  for (unsigned t = 0U; t < N_STRATEGY_TIERS; ++t) {
    ScanStrategy             strategy     = strategies[t][sig];
    const SerdStatementOrder triple_order = strategy.order;

    assert(strategy.order < SERD_ORDER_GSPO);

    if (strategy.n_prefix > 0) {
      if (with_g) {
        const SerdStatementOrder quad_order =
          (SerdStatementOrder)(triple_order + 6U);

        if (model->indices[quad_order]) {
          strategy.order = quad_order;
          ++strategy.n_prefix;
          return strategy;
        }
      }

      if (model->indices[triple_order]) {
        return strategy;
      }
    }
  }

  // Indices don't help with the triple, try to at least stay in the graph
  if (with_g) {
    for (unsigned i = SERD_ORDER_GSPO; i <= SERD_ORDER_GPOS; ++i) {
      if (model->indices[i]) {
        const ScanMode     mode     = sig == 0x000 ? SCAN_RANGE : FILTER_RANGE;
        const ScanStrategy strategy = {mode, 1U, (SerdStatementOrder)i};

        return strategy;
      }
    }
  }

  // All is lost, regress to linear search
  const ScanStrategy linear = {FILTER_EVERYTHING, 0U, model->default_order};
  return linear;
}

static SerdCursor
serd_model_search(const SerdModel* const model,
                  const SerdNode* const  s,
                  const SerdNode* const  p,
                  const SerdNode* const  o,
                  const SerdNode* const  g)
{
  // Build a pattern of interned nodes
  const SerdNode* pattern[4] = {serd_nodes_existing(model->nodes, s),
                                serd_nodes_existing(model->nodes, p),
                                serd_nodes_existing(model->nodes, o),
                                serd_nodes_existing(model->nodes, g)};

  // If some node isn't in the model at all, no need to search for statements
  const int n_given = !!s + !!p + !!o + !!g;
  if (n_given != (!!pattern[0] + !!pattern[1] + !!pattern[2] + !!pattern[3])) {
    return model->end;
  }

  // Determine the best available search strategy
  const ScanStrategy       strategy = serd_model_strategy(model, s, p, o, g);
  const SerdStatementOrder order    = strategy.order;
  ZixBTree* const          index    = model->indices[order];

  // Issue any performance warnings, and return early for degenerate cases
  switch (strategy.mode) {
  case SCAN_EVERYTHING:
    return make_begin_cursor(model, order);
  case SCAN_RANGE:
    break;
  case FILTER_EVERYTHING:
    log_bad_index(model, "using effectively linear", order, s, p, o, g);
    return serd_cursor_make(model, zix_btree_begin(index), pattern, strategy);
  case FILTER_RANGE:
    log_bad_index(model, "filtering partial", order, s, p, o, g);
    break;
  }

  // Find the first statement matching the pattern in the index
  ZixBTreeIter iter = zix_btree_end_iter;
  zix_btree_lower_bound(index,
                        serd_model_pattern_comparator(model, order),
                        orderings[order],
                        pattern,
                        &iter);

  return (!zix_btree_iter_is_end(iter) &&
          serd_iter_in_range(iter, pattern, strategy))
           ? serd_cursor_make(model, iter, pattern, strategy)
           : model->end;
}

SerdCursor*
serd_model_find(ZixAllocator* const    allocator,
                const SerdModel* const model,
                const SerdNode* const  s,
                const SerdNode* const  p,
                const SerdNode* const  o,
                const SerdNode* const  g)
{
  assert(model);

  const SerdCursor cursor = serd_model_search(model, s, p, o, g);

  return zix_btree_iter_is_end(cursor.iter)
           ? NULL
           : serd_cursor_copy(allocator, &cursor);
}

const SerdNode*
serd_model_get(const SerdModel* const model,
               const SerdNode* const  s,
               const SerdNode* const  p,
               const SerdNode* const  o,
               const SerdNode* const  g)
{
  assert(model);

  const SerdStatementView statement =
    serd_model_get_statement(model, s, p, o, g);

  return !statement.subject ? NULL
         : !s               ? statement.subject
         : !p               ? statement.predicate
         : !o               ? statement.object
         : !g               ? statement.graph
                            : NULL;
}

SerdStatementView
serd_model_get_statement(const SerdModel* const model,
                         const SerdNode* const  s,
                         const SerdNode* const  p,
                         const SerdNode* const  o,
                         const SerdNode* const  g)
{
  assert(model);

  static const SerdStatementView no_statement = {
    NULL, NULL, NULL, NULL, {NULL, 0, 0}};

  if ((bool)s + (bool)p + (bool)o != 2 &&
      (bool)s + (bool)p + (bool)o + (bool)g != 3) {
    return no_statement;
  }

  const SerdCursor i = serd_model_search(model, s, p, o, g);

  return serd_cursor_get(&i);
}

size_t
serd_model_count(const SerdModel* const model,
                 const SerdNode* const  s,
                 const SerdNode* const  p,
                 const SerdNode* const  o,
                 const SerdNode* const  g)
{
  assert(model);

  SerdCursor i     = serd_model_search(model, s, p, o, g);
  size_t     count = 0;

  for (; !serd_cursor_is_end(&i); serd_cursor_advance(&i)) {
    ++count;
  }

  return count;
}

bool
serd_model_ask(const SerdModel* const model,
               const SerdNode* const  s,
               const SerdNode* const  p,
               const SerdNode* const  o,
               const SerdNode* const  g)
{
  assert(model);

  const SerdCursor c = serd_model_search(model, s, p, o, g);

  return !serd_cursor_is_end(&c);
}

static SerdCaret*
serd_model_intern_caret(SerdModel* const model, const SerdCaretView caret)
{
  if (!caret.document) {
    return NULL;
  }

  SerdCaret* const copy =
    (SerdCaret*)zix_calloc(model->allocator, 1, sizeof(SerdCaret));

  if (copy) {
    copy->document = serd_nodes_intern(model->nodes, caret.document);
    copy->line     = caret.line;
    copy->col      = caret.column;
  }

  return copy;
}

SerdStatus
serd_model_add_from(SerdModel* const      model,
                    const SerdNode* const s,
                    const SerdNode* const p,
                    const SerdNode* const o,
                    const SerdNode* const g,
                    const SerdCaretView   caret)
{
  assert(model);

  SerdStatement* const statement =
    serd_statement_new(model->allocator, s, p, o, g, NULL);

  if (!statement) {
    return SERD_BAD_ALLOC;
  }

  statement->caret = serd_model_intern_caret(model, caret);

  bool added = false;
  for (unsigned i = 0U; i < N_STATEMENT_ORDERS; ++i) {
    if (model->indices[i]) {
      if (!zix_btree_insert(model->indices[i], statement)) {
        added = true;
      } else if ((SerdStatementOrder)i == model->default_order) {
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
  static const SerdCaretView no_caret = {NULL, 0, 0};
  return serd_model_add_from(model,
                             serd_nodes_intern(model->nodes, s),
                             serd_nodes_intern(model->nodes, p),
                             serd_nodes_intern(model->nodes, o),
                             serd_nodes_intern(model->nodes, g),
                             no_caret);
}

SerdStatus
serd_model_insert(SerdModel* const model, const SerdStatementView statement)
{
  SerdNodes* const nodes = model->nodes;

  const SerdCaretView caret = {
    serd_nodes_intern(nodes, statement.caret.document),
    statement.caret.line,
    statement.caret.column};

  return serd_model_add_from(model,
                             serd_nodes_intern(nodes, statement.subject),
                             serd_nodes_intern(nodes, statement.predicate),
                             serd_nodes_intern(nodes, statement.object),
                             serd_nodes_intern(nodes, statement.graph),
                             caret);
}

SerdStatus
serd_model_insert_statements(SerdModel* const model, SerdCursor* const range)
{
  SerdStatementView statement = serd_cursor_get(range);
  SerdStatus        st        = SERD_SUCCESS;

  while (!st && statement.subject) {
    if (!(st = serd_model_insert(model, statement))) {
      st = serd_cursor_advance(range);
    }

    statement = serd_cursor_get(range);
  }

  return st;
}

SerdStatus
serd_model_erase(SerdModel* const model, SerdCursor* const cursor)
{
  assert(model);
  assert(cursor);

  const SerdStatement* statement = serd_cursor_get_internal(cursor);
  SerdStatement*       removed   = NULL;
  ZixStatus            zst       = ZIX_STATUS_SUCCESS;

  if (!statement) {
    return SERD_FAILURE;
  }

  // Erase from the index associated with this cursor
  zst = zix_btree_remove(model->indices[cursor->strategy.order],
                         statement,
                         (void**)&removed,
                         &cursor->iter);

  // No error should be possible with a valid cursor
  assert(!zst);
  assert(removed);
  assert(removed == statement);

  for (unsigned i = SERD_ORDER_SPO; i <= SERD_ORDER_GPOS; ++i) {
    ZixBTree* index = model->indices[i];

    if (index && i != (unsigned)cursor->strategy.order) {
      SerdStatement* ignored = NULL;
      ZixBTreeIter   next    = zix_btree_end_iter;
      zst = zix_btree_remove(index, statement, (void**)&ignored, &next);

      assert(zst == ZIX_STATUS_SUCCESS || zst == ZIX_STATUS_NOT_FOUND);
      assert(!ignored || ignored == removed);
    }
  }
  serd_cursor_scan_next(cursor);

  serd_model_drop_statement(model, removed);
  cursor->version = ++model->version;

  (void)zst;
  return SERD_SUCCESS;
}

SerdStatus
serd_model_erase_statements(SerdModel* const model, SerdCursor* const range)
{
  assert(model);
  assert(range);

  SerdStatus st = SERD_SUCCESS;

  while (!st && !serd_cursor_is_end(range)) {
    st = serd_model_erase(model, range);
  }

  return st;
}

SerdStatus
serd_model_clear(SerdModel* const model)
{
  assert(model);

  SerdStatus st = SERD_SUCCESS;
  SerdCursor i  = make_begin_cursor(model, model->default_order);

  while (!st && !serd_cursor_is_end(&i)) {
    st = serd_model_erase(model, &i);
  }

  return st;
}
