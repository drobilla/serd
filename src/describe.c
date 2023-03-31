// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "cursor.h"
#include "model.h"
#include "world_impl.h"

// Define the types used in the hash interface for more type safety
#define ZIX_HASH_KEY_TYPE const SerdNode
#define ZIX_HASH_RECORD_TYPE const SerdNode

#include "serd/cursor.h"
#include "serd/describe.h"
#include "serd/event.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/digest.h"
#include "zix/hash.h"
#include "zix/status.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum { NAMED, ANON_S, ANON_O, LIST_S, LIST_O } NodeStyle;

typedef struct {
  ZixAllocator*     allocator;     // Allocator for auxiliary structures
  const SerdModel*  model;         // Model to read from
  const SerdSink*   sink;          // Sink to write description to
  ZixHash*          list_subjects; // Nodes written in the current list or null
  SerdDescribeFlags flags;         // Flags to control description
} DescribeContext;

static SerdStatus
write_range_statement(const DescribeContext*  ctx,
                      unsigned                depth,
                      SerdStatementEventFlags statement_flags,
                      SerdStatementView       statement,
                      const SerdNode*         last_subject,
                      bool                    write_types);

static NodeStyle
get_node_style(const SerdModel* const model, const SerdNode* const node)
{
  if (serd_node_type(node) != SERD_BLANK) {
    return NAMED; // Non-blank node can't be anonymous
  }

  const size_t n_as_object = serd_model_count(model, NULL, NULL, node, NULL);
  if (n_as_object > 1) {
    return NAMED; // Blank node referred to several times
  }

  if (serd_model_count(model, node, model->world->rdf_first, NULL, NULL) == 1 &&
      serd_model_count(model, node, model->world->rdf_rest, NULL, NULL) == 1 &&
      !serd_model_ask(model, NULL, model->world->rdf_rest, node, NULL)) {
    return n_as_object == 0 ? LIST_S : LIST_O;
  }

  return n_as_object == 0 ? ANON_S : ANON_O;
}

static const SerdNode*
identity(const SerdNode* const record)
{
  return record;
}

static ZixHashCode
ptr_hash(const SerdNode* const ptr)
{
  return zix_digest(0U, &ptr, sizeof(SerdNode*));
}

static bool
ptr_equals(const SerdNode* const a, const SerdNode* const b)
{
  return *(const void* const*)a == *(const void* const*)b;
}

static SerdStatus
write_pretty_range(const DescribeContext* const ctx,
                   const unsigned               depth,
                   SerdCursor* const            range,
                   const SerdNode*              last_subject,
                   bool                         write_types)
{
  SerdStatus st = SERD_SUCCESS;

  while (!st && !serd_cursor_is_end(range)) {
    const SerdStatementView statement = serd_cursor_get(range);

    // Write this statement (and possibly more to describe anonymous nodes)
    if ((st = write_range_statement(
           ctx, depth, 0U, statement, last_subject, write_types))) {
      break;
    }

    // Update the last subject and advance the cursor
    last_subject = statement.subject;
    st           = serd_cursor_advance(range);
  }

  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
write_list(const DescribeContext* const ctx,
           const unsigned               depth,
           SerdStatementEventFlags      flags,
           const SerdNode*              node,
           const SerdNode* const        graph)
{
  const SerdModel* const model     = ctx->model;
  const SerdWorld* const world     = model->world;
  const SerdSink* const  sink      = ctx->sink;
  const SerdNode* const  rdf_first = world->rdf_first;
  const SerdNode* const  rdf_rest  = world->rdf_rest;
  const SerdNode* const  rdf_nil   = world->rdf_nil;
  SerdStatus             st        = SERD_SUCCESS;

  SerdStatementView fs =
    serd_model_get_statement(model, node, rdf_first, NULL, graph);

  assert(fs.subject); // Shouldn't get here without at least an rdf:first

  while (!st && !serd_node_equals(node, rdf_nil)) {
    // Write rdf:first statement for this node
    if ((st = write_range_statement(ctx, depth, flags, fs, NULL, false))) {
      return st;
    }

    // Get rdf:rest statement
    const SerdStatementView rs =
      serd_model_get_statement(model, node, rdf_rest, NULL, graph);

    if (!rs.subject) {
      // Terminate malformed list with missing rdf:rest
      return serd_sink_write(sink, 0, node, rdf_rest, rdf_nil, graph);
    }

    // Get rdf:first statement
    const SerdNode* const next = rs.object;
    fs = serd_model_get_statement(model, next, rdf_first, NULL, graph);

    // Terminate if the next node has no rdf:first
    if (!fs.subject) {
      return serd_sink_write(sink, 0, node, rdf_rest, rdf_nil, graph);
    }

    // Write rdf:next statement and move to the next node
    st    = serd_sink_write_statement(sink, 0, rs);
    node  = next;
    flags = 0U;
  }

  return st;
}

static bool
skip_range_statement(const SerdModel* const  model,
                     const SerdStatementView statement)
{
  const NodeStyle subject_style = get_node_style(model, statement.subject);

  if (subject_style == ANON_O || subject_style == LIST_O) {
    return true; // Skip subject that will be inlined elsewhere
  }

  if (subject_style == LIST_S &&
      (serd_node_equals(statement.predicate, model->world->rdf_first) ||
       serd_node_equals(statement.predicate, model->world->rdf_rest))) {
    return true; // Skip list statement that write_list will handle
  }

  return false;
}

static SerdStatus
write_subject_types(const DescribeContext* const ctx,
                    const unsigned               depth,
                    const SerdNode* const        subject,
                    const SerdNode* const        graph)
{
  SerdCursor* const t = serd_model_find(ctx->allocator,
                                        ctx->model,
                                        subject,
                                        ctx->model->world->rdf_type,
                                        NULL,
                                        graph);

  const SerdStatus st =
    t ? write_pretty_range(ctx, depth + 1, t, subject, true) : SERD_SUCCESS;

  serd_cursor_free(ctx->allocator, t);
  return st;
}

static bool
types_first_for_subject(const DescribeContext* const ctx, const NodeStyle style)
{
  return style != LIST_S && !(ctx->flags & SERD_NO_TYPE_FIRST);
}

static SerdStatus
write_range_statement(const DescribeContext* const ctx,
                      const unsigned               depth,
                      SerdStatementEventFlags      statement_flags,
                      const SerdStatementView      statement,
                      const SerdNode* ZIX_NULLABLE last_subject,
                      const bool                   write_types)
{
  const SerdModel* const model         = ctx->model;
  const SerdSink* const  sink          = ctx->sink;
  const SerdNode* const  subject       = statement.subject;
  const NodeStyle        subject_style = get_node_style(model, subject);
  const SerdNode* const  predicate     = statement.predicate;
  const SerdNode* const  object        = statement.object;
  const NodeStyle        object_style  = get_node_style(model, object);
  const SerdNode* const  graph         = statement.graph;
  SerdStatus             st            = SERD_SUCCESS;

  if (depth == 0U) {
    if (skip_range_statement(model, statement)) {
      return SERD_SUCCESS; // Skip subject that will be inlined elsewhere
    }

    if (subject_style == LIST_S) {
      // First write inline list subject, which this statement will follow
      if (zix_hash_insert(ctx->list_subjects, subject) != ZIX_STATUS_EXISTS) {
        st = write_list(ctx, 2, statement_flags | SERD_LIST_S, subject, graph);
      }
    }
  }

  if (st) {
    return st;
  }

  // If this is a new subject, write types first if necessary
  const bool types_first = types_first_for_subject(ctx, subject_style);
  if (subject != last_subject && types_first) {
    st = write_subject_types(ctx, depth, subject, graph);
  }

  // Skip type statement if it would be written another time (just above)
  if (subject_style != LIST_S && !write_types &&
      serd_node_equals(predicate, model->world->rdf_type)) {
    return st;
  }

  // Set up the flags for this statement
  statement_flags |=
    (((subject_style == ANON_S) * (SerdStatementEventFlags)SERD_EMPTY_S) |
     ((object_style == ANON_O) * (SerdStatementEventFlags)SERD_ANON_O) |
     ((object_style == LIST_O) * (SerdStatementEventFlags)SERD_LIST_O));

  // Finally write this statement
  if ((st = serd_sink_write_statement(sink, statement_flags, statement))) {
    return st;
  }

  if (object_style == ANON_O) {
    // Follow an anonymous object with its description like "[ ... ]"
    SerdCursor* const iter =
      serd_model_find(ctx->allocator, model, object, NULL, NULL, NULL);

    if (!(st = write_pretty_range(ctx, depth + 1, iter, last_subject, false))) {
      st = serd_sink_write_end(sink, object);
    }

    serd_cursor_free(ctx->allocator, iter);

  } else if (object_style == LIST_O) {
    // Follow a list object with its description like "( ... )"
    st = write_list(ctx, depth + 1, 0U, object, graph);
  }

  return st;
}

SerdStatus
serd_describe_range(ZixAllocator* const     allocator,
                    const SerdCursor* const range,
                    const SerdSink*         sink,
                    const SerdDescribeFlags flags)
{
  if (!range || serd_cursor_is_end(range)) {
    return SERD_SUCCESS;
  }

  assert(sink);

  SerdCursor copy = *range;

  ZixHash* const list_subjects =
    zix_hash_new(allocator, identity, ptr_hash, ptr_equals);

  SerdStatus st = SERD_BAD_ALLOC;
  if (list_subjects) {
    DescribeContext ctx = {allocator, range->model, sink, list_subjects, flags};

    st = write_pretty_range(&ctx, 0, &copy, NULL, (flags & SERD_NO_TYPE_FIRST));
  }

  zix_hash_free(list_subjects);
  return st;
}
