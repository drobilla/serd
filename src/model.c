// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "carets.h"
#include "compare.h"
#include "cursor_impl.h"
#include "cursor_internal.h"
#include "log_internal.h"
#include "model_impl.h"
#include "model_internal.h"
#include "nodes_internal.h"
#include "orderings.h"
#include "statement.h"

#include <serd/caret_view.h>
#include <serd/cursor.h>
#include <serd/field.h>
#include <serd/log.h>
#include <serd/model.h>
#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/status.h>
#include <serd/struct_literal.h>
#include <serd/tuple.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/btree.h>
#include <zix/status.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static const SerdNodeID everything_pattern[4] = {0U, 0U, 0U, 0U};

/// A 4-bit signature for a triple pattern used as a table index
typedef enum {
  SERD_SIGNATURE_XXX,  // 0000 (???)
  SERD_SIGNATURE_XXO,  // 0001 (??O)
  SERD_SIGNATURE_XPX,  // 0010 (?P?)
  SERD_SIGNATURE_XPO,  // 0011 (?PO)
  SERD_SIGNATURE_SXX,  // 0100 (S??)
  SERD_SIGNATURE_SXO,  // 0101 (S?O)
  SERD_SIGNATURE_SPX,  // 0110 (SP?)
  SERD_SIGNATURE_SPO,  // 0111 (SPO)
  SERD_SIGNATURE_GXXX, // 1000 (G???)
  SERD_SIGNATURE_GXXO, // 1001 (G??O)
  SERD_SIGNATURE_GXPX, // 1010 (G?P?)
  SERD_SIGNATURE_GXPO, // 1011 (G?PO)
  SERD_SIGNATURE_GSXX, // 1100 (GS??)
  SERD_SIGNATURE_GSXO, // 1101 (GS?O)
  SERD_SIGNATURE_GSPX, // 1110 (GSP?)
  SERD_SIGNATURE_GSPO, // 1111 (GSPO)
} PatternSignature;

static PatternSignature
pattern_signature(const bool with_s,
                  const bool with_p,
                  const bool with_o,
                  const bool with_g)
{
  return (PatternSignature)(((unsigned)with_g << 3U) |
                            ((unsigned)with_s << 2U) |
                            ((unsigned)with_p << 1U) | (unsigned)with_o);
}

static ZixBTreeCompareFunc
serd_model_index_comparator(const SerdModel* const   model,
                            const SerdStatementOrder order)
{
  return (order < SERD_ORDER_GSPO && !(model->flags & SERD_MODEL_GRAPHS))
           ? serd_triple_compare
           : serd_quad_compare;
}

static ZixBTreeCompareFunc
serd_model_pattern_comparator(const SerdModel* const   model,
                              const SerdStatementOrder order)
{
  return (order < SERD_ORDER_GSPO && !(model->flags & SERD_MODEL_GRAPHS))
           ? serd_triple_compare_pattern
           : serd_quad_compare_pattern;
}

static SerdStatus
zix_to_serd_status(const ZixStatus st)
{
  return (st == ZIX_STATUS_SUCCESS)  ? SERD_SUCCESS
         : (st == ZIX_STATUS_NO_MEM) ? SERD_BAD_ALLOC
         : (st == ZIX_STATUS_EXISTS) ? SERD_FAILURE
                                     : SERD_UNKNOWN_ERROR;
}

SerdStatus
serd_model_add_index(SerdModel* const model, const SerdStatementOrder order)
{
  assert(model);

  if (model->indices[order]) {
    return SERD_FAILURE;
  }

  if (order >= SERD_ORDER_GSPO && !(model->flags & SERD_MODEL_GRAPHS)) {
    return SERD_BAD_ARG;
  }

  const ZixBTreeCompareFunc comparator =
    serd_model_index_comparator(model, order);

  ZixBTree* const index =
    zix_btree_new(model->allocator, comparator, &model->cmp_data[order]);
  if (!(model->indices[order] = index)) {
    return SERD_BAD_ALLOC;
  }

  // Insert statements from the default index
  ZixStatus zst = ZIX_STATUS_SUCCESS;
  if (order != model->default_order) {
    ZixBTree* const default_index = model->indices[model->default_order];
    for (ZixBTreeIter i = zix_btree_begin(default_index);
         !zst && !zix_btree_iter_is_end(i);
         zix_btree_iter_increment(&i)) {
      zst = zix_btree_insert(index, zix_btree_get(i));
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

bool
serd_model_has_index(const SerdModel* const   model,
                     const SerdStatementOrder order)
{
  return model->indices[order];
}

static SerdModel*
serd_model_new_with_allocator(ZixAllocator* const      allocator,
                              SerdWorld* const         world,
                              SerdNodes* const         nodes,
                              const SerdStatementOrder default_order,
                              const SerdModelFlags     flags)
{
  assert(world);

  SerdModel* model =
    (SerdModel*)zix_calloc(allocator, 1, sizeof(struct SerdModelImpl));

  if (!model) {
    return NULL;
  }

  if (flags & SERD_MODEL_CARETS) {
    if (!(model->carets = serd_carets_new(allocator))) {
      zix_free(allocator, model);
      return NULL;
    }
  }

  model->allocator     = allocator;
  model->world         = world;
  model->nodes         = nodes;
  model->default_order = default_order;
  model->flags         = flags;

  for (unsigned i = 0U; i < 12U; ++i) {
    model->cmp_data[i].nodes = model->nodes;
    for (unsigned j = 0U; j < 4U; ++j) {
      model->cmp_data[i].ordering[j] = orderings[i][j];
    }
  }

  if (serd_model_add_index(model, default_order)) {
    serd_carets_free(model->carets, model->allocator);
    zix_free(allocator, model);
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
               SerdNodes* const         nodes,
               const SerdStatementOrder default_order,
               const SerdModelFlags     flags)
{
  return serd_model_new_with_allocator(
    serd_world_allocator(world), world, nodes, default_order, flags);
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
            serd_no_caret(),
            "%s index (%s) for (%s %s %s%s) query",
            description,
            order_strings[index_order],
            s ? "S" : "?",
            p ? "P" : "?",
            o ? "O" : "?",
            (model->flags & SERD_MODEL_GRAPHS) ? g ? " G" : " ?" : "");
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
serd_model_copy(ZixAllocator* const    allocator,
                SerdNodes* const       nodes,
                const SerdModel* const model)
{
  if (!model) {
    return NULL;
  }

  SerdModel* copy = serd_model_new_with_allocator(
    allocator, model->world, nodes, model->default_order, model->flags);
  if (!copy) {
    return NULL;
  }

  SerdCursor       cursor = make_begin_cursor(model, model->default_order);
  const SerdStatus st     = serd_model_insert_range(copy, &cursor);
  if (st > SERD_FAILURE) {
    serd_model_free(copy);
    return NULL;
  }

  assert(serd_model_size(copy) == serd_model_size(model));
  return copy;
}

static bool
statement_equals(const SerdNodes* const     a_nodes,
                 const SerdStatement* const a,
                 const SerdNodes* const     b_nodes,
                 const SerdStatement* const b)
{
  assert(a);
  assert(b);
  assert(a != b);

  return (a_nodes == b_nodes)
           ? (a->nodes[0] == b->nodes[0] && a->nodes[1] == b->nodes[1] &&
              a->nodes[2] == b->nodes[2] && a->nodes[3] == b->nodes[3])
           : (serd_nodes_equals_foreign_token(
                a_nodes, a->nodes[0], b_nodes, b->nodes[0]) &&
              serd_nodes_equals_foreign_token(
                a_nodes, a->nodes[1], b_nodes, b->nodes[1]) &&
              serd_nodes_equals_foreign_object(
                a_nodes, a->nodes[2], b_nodes, b->nodes[2]) &&
              serd_nodes_equals_foreign_token(
                a_nodes, a->nodes[3], b_nodes, b->nodes[3]));
}

bool
serd_model_equals(const SerdModel* const lhs, const SerdModel* const rhs)
{
  if (lhs == rhs) {
    return true;
  }

  if (!lhs || !rhs || serd_model_size(lhs) != serd_model_size(rhs)) {
    return false;
  }

  SerdCursor l = make_begin_cursor(lhs, lhs->default_order);
  SerdCursor r = make_begin_cursor(rhs, rhs->default_order);

  while (!serd_cursor_is_end(&l) && !serd_cursor_is_end(&r)) {
    if (!statement_equals(lhs->nodes,
                          serd_cursor_get_internal(&l),
                          rhs->nodes,
                          serd_cursor_get_internal(&r))) {
      return false;
    }

    serd_cursor_advance(&l);
    serd_cursor_advance(&r);
  }

  return serd_cursor_is_end(&l) && serd_cursor_is_end(&r);
}

static void
serd_model_drop_statement(SerdModel* const     model,
                          SerdStatement* const statement)
{
  assert(statement);

  serd_carets_remove(model->carets, model->allocator, statement);
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

  serd_carets_free(model->carets, model->allocator);
  zix_free(model->allocator, model);
}

SerdWorld*
serd_model_world(const SerdModel* const model)
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

SerdNodes*
serd_model_mutable_nodes(SerdModel* const model)
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

static ScanStrategy
triple_strategy(const SerdModel* const model,
                const bool             s,
                const bool             p,
                const bool             o)
{
#define N_STRATEGY_TIERS 6U

  assert(s || p || o);

  static const ScanStrategy none = {SCAN_EVERYTHING, 0U, SERD_ORDER_SPO};

  static const ScanStrategy strategies[8][N_STRATEGY_TIERS] = {
    // ???
    {none, none, none, none, none, none},

    // ??O
    {{SCAN_RANGE, 1U, SERD_ORDER_OPS},
     {SCAN_RANGE, 1U, SERD_ORDER_OSP},
     none,
     none,
     none,
     none},

    // ?P?
    {{SCAN_RANGE, 1U, SERD_ORDER_PSO},
     {SCAN_RANGE, 1U, SERD_ORDER_POS},
     none,
     none,
     none,
     none},

    // ?PO
    {{SCAN_RANGE, 2U, SERD_ORDER_OPS},
     {SCAN_RANGE, 2U, SERD_ORDER_POS},
     {FILTER_RANGE, 1U, SERD_ORDER_PSO},
     {FILTER_RANGE, 1U, SERD_ORDER_OSP},
     none,
     none},

    // S??
    {{SCAN_RANGE, 1U, SERD_ORDER_SPO},
     {SCAN_RANGE, 1U, SERD_ORDER_SOP},
     none,
     none,
     none,
     none},

    // S?O
    {{SCAN_RANGE, 2U, SERD_ORDER_SOP},
     {SCAN_RANGE, 2U, SERD_ORDER_OSP},
     {FILTER_RANGE, 1U, SERD_ORDER_SPO},
     {FILTER_RANGE, 1U, SERD_ORDER_OPS},
     none,
     none},

    // SP?
    {{SCAN_RANGE, 2U, SERD_ORDER_SPO},
     {SCAN_RANGE, 2U, SERD_ORDER_PSO},
     {FILTER_RANGE, 1U, SERD_ORDER_SOP},
     {FILTER_RANGE, 1U, SERD_ORDER_POS},
     none,
     none},

    // SPO
    {{SCAN_RANGE, 3U, SERD_ORDER_SPO},
     {SCAN_RANGE, 3U, SERD_ORDER_SOP},
     {SCAN_RANGE, 3U, SERD_ORDER_OPS},
     {SCAN_RANGE, 3U, SERD_ORDER_OSP},
     {SCAN_RANGE, 3U, SERD_ORDER_PSO},
     {SCAN_RANGE, 3U, SERD_ORDER_POS}},
  };

  // Search for a strategy an index can support, from most to least preferred
  const PatternSignature sig = pattern_signature(s, p, o, false);
  assert(sig < 8U);
  for (unsigned t = 0U; t < N_STRATEGY_TIERS; ++t) {
    const ScanStrategy strategy = strategies[sig][t];
    if (strategy.n_prefix && model->indices[strategy.order]) {
      return strategy;
    }
  }

  return none;

#undef N_STRATEGY_TIERS
}

static ScanStrategy
quad_strategy(const SerdModel* const model,
              const bool             s,
              const bool             p,
              const bool             o,
              const bool             g)
{
#define N_STRATEGY_TIERS 12U

  assert(g);

  static const ScanStrategy none = {SCAN_EVERYTHING, 0U, SERD_ORDER_GSPO};

  static const ScanStrategy strategies[8][N_STRATEGY_TIERS] = {
    // G???
    {{SCAN_RANGE, 1U, SERD_ORDER_GSPO},
     {SCAN_RANGE, 1U, SERD_ORDER_GSOP},
     {SCAN_RANGE, 1U, SERD_ORDER_GOPS},
     {SCAN_RANGE, 1U, SERD_ORDER_GOSP},
     {SCAN_RANGE, 1U, SERD_ORDER_GPSO},
     {SCAN_RANGE, 1U, SERD_ORDER_GPOS},
     none,
     none,
     none,
     none,
     none,
     none},

    // G??O
    {{SCAN_RANGE, 2U, SERD_ORDER_GOPS},
     {SCAN_RANGE, 2U, SERD_ORDER_GOSP},
     {FILTER_RANGE, 1U, SERD_ORDER_OPS},
     {FILTER_RANGE, 1U, SERD_ORDER_OSP},
     {FILTER_RANGE, 1U, SERD_ORDER_GSPO},
     {FILTER_RANGE, 1U, SERD_ORDER_GSOP},
     {FILTER_RANGE, 1U, SERD_ORDER_GPSO},
     {FILTER_RANGE, 1U, SERD_ORDER_GPOS},
     none,
     none,
     none,
     none},

    // G?P?
    {{SCAN_RANGE, 2U, SERD_ORDER_GPSO},
     {SCAN_RANGE, 2U, SERD_ORDER_GPOS},
     {FILTER_RANGE, 1U, SERD_ORDER_PSO},
     {FILTER_RANGE, 1U, SERD_ORDER_POS},
     {FILTER_RANGE, 1U, SERD_ORDER_GSPO},
     {FILTER_RANGE, 1U, SERD_ORDER_GSOP},
     {FILTER_RANGE, 1U, SERD_ORDER_GOPS},
     {FILTER_RANGE, 1U, SERD_ORDER_GOSP},
     none,
     none,
     none,
     none},

    // G?PO
    {{SCAN_RANGE, 3U, SERD_ORDER_GOPS},
     {SCAN_RANGE, 3U, SERD_ORDER_GPOS},
     {FILTER_RANGE, 2U, SERD_ORDER_OPS},
     {FILTER_RANGE, 2U, SERD_ORDER_POS},
     {FILTER_RANGE, 2U, SERD_ORDER_GOSP},
     {FILTER_RANGE, 2U, SERD_ORDER_GPSO},
     {FILTER_RANGE, 1U, SERD_ORDER_OSP},
     {FILTER_RANGE, 1U, SERD_ORDER_PSO},
     {FILTER_RANGE, 1U, SERD_ORDER_GSPO},
     {FILTER_RANGE, 1U, SERD_ORDER_GSOP},
     none,
     none},

    // GS??
    {{SCAN_RANGE, 2U, SERD_ORDER_GSPO},
     {SCAN_RANGE, 2U, SERD_ORDER_GSOP},
     {FILTER_RANGE, 1U, SERD_ORDER_SPO},
     {FILTER_RANGE, 1U, SERD_ORDER_SOP},
     {FILTER_RANGE, 1U, SERD_ORDER_GOPS},
     {FILTER_RANGE, 1U, SERD_ORDER_GOSP},
     {FILTER_RANGE, 1U, SERD_ORDER_GPSO},
     {FILTER_RANGE, 1U, SERD_ORDER_GPOS},
     none,
     none,
     none,
     none},

    // GS?O
    {{SCAN_RANGE, 3U, SERD_ORDER_GSOP},
     {SCAN_RANGE, 3U, SERD_ORDER_GOSP},
     {FILTER_RANGE, 2U, SERD_ORDER_SOP},
     {FILTER_RANGE, 2U, SERD_ORDER_OSP},
     {FILTER_RANGE, 2U, SERD_ORDER_GSPO},
     {FILTER_RANGE, 2U, SERD_ORDER_GOPS},
     {FILTER_RANGE, 1U, SERD_ORDER_SPO},
     {FILTER_RANGE, 1U, SERD_ORDER_OPS},
     {FILTER_RANGE, 1U, SERD_ORDER_GPSO},
     {FILTER_RANGE, 1U, SERD_ORDER_GPOS},
     none,
     none},

    // GSP?
    {{SCAN_RANGE, 3U, SERD_ORDER_GSPO},
     {SCAN_RANGE, 3U, SERD_ORDER_GPSO},
     {FILTER_RANGE, 2U, SERD_ORDER_SPO},
     {FILTER_RANGE, 2U, SERD_ORDER_PSO},
     {FILTER_RANGE, 2U, SERD_ORDER_GSOP},
     {FILTER_RANGE, 2U, SERD_ORDER_GPOS},
     {FILTER_RANGE, 1U, SERD_ORDER_SOP},
     {FILTER_RANGE, 1U, SERD_ORDER_POS},
     {FILTER_RANGE, 1U, SERD_ORDER_GOPS},
     {FILTER_RANGE, 1U, SERD_ORDER_GOSP},
     none,
     none},

    // GSPO
    {{SCAN_RANGE, 4U, SERD_ORDER_GSPO},
     {SCAN_RANGE, 4U, SERD_ORDER_GSOP},
     {SCAN_RANGE, 4U, SERD_ORDER_GOPS},
     {SCAN_RANGE, 4U, SERD_ORDER_GOSP},
     {SCAN_RANGE, 4U, SERD_ORDER_GPSO},
     {SCAN_RANGE, 4U, SERD_ORDER_GPOS},
     {FILTER_RANGE, 3U, SERD_ORDER_SPO},
     {FILTER_RANGE, 3U, SERD_ORDER_SOP},
     {FILTER_RANGE, 3U, SERD_ORDER_OPS},
     {FILTER_RANGE, 3U, SERD_ORDER_OSP},
     {FILTER_RANGE, 3U, SERD_ORDER_PSO},
     {FILTER_RANGE, 3U, SERD_ORDER_POS}},
  };

  // Search for a strategy an index can support, from most to least preferred
  const PatternSignature sig = pattern_signature(s, p, o, g);
  assert(sig >= 8U);
  assert(sig < 16U);
  const unsigned strategy_index = (unsigned)sig - 8U;
  for (unsigned t = 0U; t < N_STRATEGY_TIERS; ++t) {
    const ScanStrategy strategy = strategies[strategy_index][t];
    if (strategy.n_prefix && model->indices[strategy.order]) {
      return strategy;
    }
  }

  return none;

#undef N_STRATEGY_TIERS
}

/// Return the best index scanning strategy for a pattern with given nodes
static ScanStrategy
scan_strategy(const SerdModel* const model,
              const bool             s,
              const bool             p,
              const bool             o,
              const bool             g)
{
  // Try to find a good strategy supported by an index
  ScanStrategy strat =
    g ? quad_strategy(model, s, p, o, g) : triple_strategy(model, s, p, o);

  if (strat.mode == SCAN_EVERYTHING) {
    // Regress to linear search
    log_bad_index(model, "using effectively linear", strat.order, s, p, o, g);
    strat.mode  = FILTER_EVERYTHING;
    strat.order = model->default_order;
  }

  if (strat.mode == FILTER_RANGE) {
    log_bad_index(model, "filtering partial", strat.order, s, p, o, g);
  }

  return strat;
}

SerdCursor
serd_model_find_internal(const SerdModel* const model,
                         const SerdNodeID       s,
                         const SerdNodeID       p,
                         const SerdNodeID       o,
                         const SerdNodeID       g)
{
  if (!s && !p && !o && !g) {
    return make_begin_cursor(model, model->default_order);
  }

  // Determine the best available search strategy
  const SerdNodeID         pattern[4] = {s, p, o, g};
  const ScanStrategy       strategy   = scan_strategy(model, s, p, o, g);
  const SerdStatementOrder order      = strategy.order;
  ZixBTree* const          index      = model->indices[order];

  assert(strategy.mode != SCAN_EVERYTHING);
  if (strategy.mode == FILTER_EVERYTHING) {
    return serd_cursor_make(model, zix_btree_begin(index), pattern, strategy);
  }

  // Find the first statement matching the pattern in the index
  ZixBTreeIter iter = zix_btree_end_iter;
  zix_btree_lower_bound(index,
                        serd_model_pattern_comparator(model, order),
                        &model->cmp_data[order],
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
                const SerdNodeID       s,
                const SerdNodeID       p,
                const SerdNodeID       o,
                const SerdNodeID       g)
{
  assert(model);

  const SerdCursor cursor = serd_model_find_internal(model, s, p, o, g);

  return zix_btree_iter_is_end(cursor.iter)
           ? NULL
           : serd_cursor_copy(allocator, &cursor);
}

SerdNodeID
serd_model_find_node(const SerdModel* const model,
                     const SerdNodeID       s,
                     const SerdNodeID       p,
                     const SerdNodeID       o,
                     const SerdNodeID       g)
{
  assert(model);

  const bool has_s = !!s;
  const bool has_p = !!p;
  const bool has_o = !!o;
  const bool has_g = !!g;
  if (has_s + has_p + has_o != 2 && has_s + has_p + has_o + has_g != 3) {
    return 0U;
  }

  const SerdCursor i = serd_model_find_internal(model, s, p, o, g);
  if (serd_cursor_is_end(&i)) {
    return 0U;
  }

  const SerdStatement* const statement = serd_cursor_get_internal(&i);
  assert(statement->nodes[0]);
  assert(statement->nodes[1]);
  assert(statement->nodes[2]);

  return !s   ? statement->nodes[0]
         : !p ? statement->nodes[1]
         : !o ? statement->nodes[2]
              : statement->nodes[3];
}

size_t
serd_model_count(const SerdModel* const model,
                 const SerdNodeID       s,
                 const SerdNodeID       p,
                 const SerdNodeID       o,
                 const SerdNodeID       g)
{
  assert(model);

  SerdCursor i     = serd_model_find_internal(model, s, p, o, g);
  size_t     count = 0;

  for (; !serd_cursor_is_end(&i); serd_cursor_advance(&i)) {
    ++count;
  }

  return count;
}

bool
serd_model_ask(const SerdModel* const model,
               const SerdNodeID       s,
               const SerdNodeID       p,
               const SerdNodeID       o,
               const SerdNodeID       g)
{
  assert(model);

  const SerdCursor c = serd_model_find_internal(model, s, p, o, g);

  return !serd_cursor_is_end(&c);
}

SerdModelCaret
serd_model_statement_caret(const SerdModel* const     model,
                           const SerdStatement* const statement)
{
  return model->carets ? serd_carets_get(model->carets, statement)
                       : SERD_STRUCT_LITERAL(SerdModelCaret, 0U, 0U, 0U);
}

static bool
can_insert_id(const SerdModel* const model,
              const SerdField        field,
              const SerdNodeID       id)
{
  if (!id) {
    return (field == SERD_GRAPH);
  }

  const SerdNodeType type = serd_nodes_type(model->nodes, id);
  return (type == SERD_VARIABLE) ? (model->flags & SERD_MODEL_VARIABLES)
                                 : serd_field_supports(field, type);
}

static ZixStatus
remove_index_statement(ZixBTree* const            tree,
                       const SerdStatement* const statement)
{
  SerdStatement*  out  = NULL;
  ZixBTreeIter    next = zix_btree_end_iter;
  const ZixStatus zst  = zix_btree_remove(tree, statement, (void**)&out, &next);
  assert(zst == ZIX_STATUS_SUCCESS || zst == ZIX_STATUS_NOT_FOUND);
  assert(!out || out == statement);
  return zst;
}

static SerdStatus
serd_model_insert_from(SerdModel* const model,
                       const SerdNodeID s,
                       const SerdNodeID p,
                       const SerdNodeID o,
                       const SerdNodeID g,
                       const SerdNodeID document,
                       const size_t     line,
                       const size_t     column)
{
  assert(model);

  if (!can_insert_id(model, SERD_SUBJECT, s) ||
      !can_insert_id(model, SERD_PREDICATE, p) ||
      !can_insert_id(model, SERD_OBJECT, o) ||
      !can_insert_id(model, SERD_GRAPH, g)) {
    return SERD_BAD_ARG;
  }

  const SerdNodeID     mg        = (model->flags & SERD_MODEL_GRAPHS) ? g : 0U;
  ZixAllocator* const  allocator = model->allocator;
  SerdStatement* const statement = serd_statement_new(allocator, s, p, o, mg);
  if (!statement) { // Initial allocation failure, no side-effect
    return SERD_BAD_ALLOC;
  }

  // Insert into default (owning) index first to check for duplicates
  ZixBTree* const default_index = model->indices[model->default_order];
  ZixStatus       zst           = zix_btree_insert(default_index, statement);
  if (zst) { // Early failure with no insertion to undo
    serd_statement_free(model->allocator, statement);
    return zix_to_serd_status(zst);
  }

  // Commit to the insertion, tracking the last mutated index order
  unsigned last = 0U;
  ++model->version;

  // Insert caret if document origins are being tracked
  zst = serd_carets_insert(
    model->carets, allocator, statement, document, line, column);

  // Insert into other indices
  for (; !zst && last < N_STATEMENT_ORDERS; ++last) {
    if (last != model->default_order && model->indices[last]) {
      zst = zix_btree_insert(model->indices[last], statement);
    }
  }

  // If there was an (unlikely) error, undo all changes
  if (zst) {
    for (unsigned i = 0U; i <= N_STATEMENT_ORDERS; ++i) {
      if (model->indices[i] && (i == model->default_order || i <= last)) {
        (void)remove_index_statement(model->indices[i], statement);
      }
    }

    (void)serd_carets_remove(model->carets, allocator, statement);
    serd_statement_free(model->allocator, statement);
  }

  return zix_to_serd_status(zst);
}

SerdStatus
serd_model_insert(SerdModel* const model,
                  const SerdNodeID s,
                  const SerdNodeID p,
                  const SerdNodeID o,
                  const SerdNodeID g)
{
  return serd_model_insert_from(model, s, p, o, g, 0U, 0U, 0U);
}

SerdStatus
serd_model_insert_tuple(SerdModel* const     model,
                        const SerdTuple      tuple,
                        const SerdModelCaret caret)
{
  return serd_model_insert_from(model,
                                tuple.nodes[0],
                                tuple.nodes[1],
                                tuple.nodes[2],
                                tuple.nodes[3],
                                caret.document,
                                caret.line,
                                caret.column);
}

SerdStatus
serd_model_insert_range(SerdModel* const model, SerdCursor* const cursor)
{
  assert(model);
  assert(cursor);

  if (cursor->model == model || serd_cursor_is_end(cursor)) {
    return SERD_SUCCESS;
  }

  const SerdNodes* const source = cursor->model->nodes;

  SerdStatus st = SERD_SUCCESS;
  while (!st) {
    const SerdStatement* statement = serd_cursor_get_internal(cursor);
    assert(statement);

    const SerdTuple tuple = {
      {serd_nodes_crib(model->nodes, source, statement->nodes[0]),
       serd_nodes_crib(model->nodes, source, statement->nodes[1]),
       serd_nodes_crib(model->nodes, source, statement->nodes[2]),
       serd_nodes_crib(model->nodes, source, statement->nodes[3])}};

    if (!tuple.nodes[0] || !tuple.nodes[1] || !tuple.nodes[2] ||
        (statement->nodes[3] && !tuple.nodes[3])) {
      st = SERD_BAD_ALLOC;
    } else {
      SerdModelCaret caret = {0U, 0U, 0U};
      if ((model->flags & SERD_MODEL_CARETS)) {
        caret = serd_cursor_caret(cursor);
      }

      st = serd_model_insert_tuple(model, tuple, caret);
      if (st <= SERD_FAILURE) {
        st = serd_cursor_advance(cursor);
      }
    }
  }

  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

SerdStatus
serd_model_erase(SerdModel* const model, SerdCursor* const cursor)
{
  assert(model);
  assert(cursor);

  const SerdStatement* statement = serd_cursor_get_internal(cursor);
  SerdStatement*       removed   = NULL;
  if (!statement) {
    return SERD_FAILURE;
  }

  // Erase from the index associated with this cursor
  ZixStatus zst = zix_btree_remove(model->indices[cursor->strategy.order],
                                   statement,
                                   (void**)&removed,
                                   &cursor->iter);
  assert(!zst);                 // No error possible with a valid cursor
  assert(removed == statement); // Removed the given statement
  (void)zst;

  // Erase from any other indices
  for (unsigned i = 0U; i < N_STATEMENT_ORDERS; ++i) {
    if (model->indices[i] && i != (unsigned)cursor->strategy.order) {
      (void)remove_index_statement(model->indices[i], statement);
    }
  }

  serd_cursor_scan_next(cursor);
  serd_model_drop_statement(model, removed);
  cursor->version = ++model->version;
  return SERD_SUCCESS;
}

SerdStatus
serd_model_erase_range(SerdModel* const model, SerdCursor* const cursor)
{
  assert(model);
  assert(cursor);

  SerdStatus st = SERD_SUCCESS;

  while (!st && !serd_cursor_is_end(cursor)) {
    st = serd_model_erase(model, cursor);
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
