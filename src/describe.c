// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "cursor_impl.h"
#include "cursor_internal.h"
#include "model_internal.h"
#include "namespaces.h"
#include "statement.h"

#include <serd/cursor.h>
#include <serd/describe.h>
#include <serd/event.h>
#include <serd/model.h>
#include <serd/node_args.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/strings.h>
#include <serd/token_view.h>
#include <serd/tuple.h>
#include <zix/allocator.h>
#include <zix/digest.h>
#include <zix/hash.h>
#include <zix/status.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  NAMED,
  ANON_G,
  ANON_S,
  ANON_O,
  EMPTY_O,
  LIST_S,
  LIST_O
} NodeStyle;

typedef struct {
  const SerdModel*   model;         // Model to read from
  const SerdSink*    sink;          // Sink to write description to
  SerdStrings*       strings;       // Strings for emitting node views
  ZixHash*           list_subjects; // Nodes written in the current list or null
  SerdNodeID         rdf_first_id;  // rdf:first
  SerdNodeID         rdf_rest_id;   // rdf:rest
  SerdNodeID         rdf_type_id;   // rdf:type
  SerdNodeID         rdf_nil_id;    // rdf:nil
  SerdStatementOrder order;         // Statement order of cursor
  SerdDescribeFlags  flags;         // Flags to control description
} DescribeContext;

static SerdStatus
write_range_statement(const DescribeContext* ctx,
                      unsigned               depth,
                      SerdEventFlags         statement_flags,
                      const SerdCursor*      cursor,
                      SerdNodeID             last_subject,
                      bool                   write_types);

static SerdStatus
emit_statement(const DescribeContext* const ctx,
               const SerdEventFlags         flags,
               const SerdCursor* const      cursor)
{
  const SerdStatement* const statement = serd_cursor_get_internal(cursor);
  SerdStrings* const         strings   = ctx->strings;

  SerdEvent event = serd_statement_event(
    flags,
    serd_quad_view(serd_strings_token(strings, statement->nodes[0]),
                   serd_strings_token(strings, statement->nodes[1]),
                   serd_strings_object(strings, statement->nodes[2]),
                   serd_strings_token(strings, statement->nodes[3])));

  if (ctx->flags & SERD_DESCRIBE_CARET) {
    event.caret = serd_strings_caret(strings, serd_cursor_caret(cursor));
  }

  return serd_sink_event(ctx->sink, event);
}

static bool
is_list_head(const DescribeContext* const ctx, const SerdNodeID id)
{
  return (!serd_model_ask(ctx->model, 0U, ctx->rdf_rest_id, id, 0U) &&
          serd_model_count(ctx->model, id, ctx->rdf_first_id, 0U, 0U) == 1 &&
          serd_model_count(ctx->model, id, ctx->rdf_rest_id, 0U, 0U) == 1);
}

static NodeStyle
get_node_style(const DescribeContext* const ctx, const SerdNodeID node_id)
{
  const SerdModel* const model = ctx->model;
  const SerdNodes* const nodes = serd_model_nodes(model);

  if (!node_id || serd_nodes_type(nodes, node_id) != SERD_BLANK) {
    return NAMED; // Non-blank node can't be anonymous
  }

  const size_t n_as_object = serd_model_count(model, 0U, 0U, node_id, 0U);
  if (n_as_object > 1) {
    return NAMED; // Blank node used as an object several times
  }

  if (ctx->order >= SERD_ORDER_GSPO) {
    const size_t n_as_graph = serd_model_count(model, 0U, 0U, 0U, node_id);
    if (n_as_graph > 0 && n_as_object == 0U) {
      return ANON_G;
    }
  }

  if (is_list_head(ctx, node_id)) {
    if (n_as_object) {
      return LIST_O;
    }

    if (serd_model_count(model, node_id, 0U, 0U, 0U) > 2) {
      return LIST_S;
    }
  }

  return n_as_object == 0                             ? ANON_S
         : serd_model_ask(model, node_id, 0U, 0U, 0U) ? ANON_O
                                                      : EMPTY_O;
}

static const void*
identity(const void* const record)
{
  return record;
}

static ZixHashCode
id_hash(const void* const ptr)
{
  const uintptr_t id = (uintptr_t)ptr;
  return zix_digest(0U, &id, sizeof(id));
}

static bool
id_equals(const void* const a, const void* const b)
{
  return (uintptr_t)a == (uintptr_t)b;
}

static SerdStatus
write_range(const DescribeContext* const ctx,
            const unsigned               depth,
            SerdCursor* const            range,
            SerdNodeID                   last_subject,
            bool                         write_types)
{
  SerdStatus st = SERD_SUCCESS;

  while (!st && !serd_cursor_is_end(range)) {
    // Write this statement (and possibly more to describe anonymous nodes)
    if ((st = write_range_statement(
           ctx, depth, 0U, range, last_subject, write_types))) {
      break;
    }

    // Update the last subject and advance the cursor
    last_subject = serd_cursor_tuple(range).nodes[0];
    st           = serd_cursor_advance(range);
  }

  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
write_list_end(const DescribeContext* const ctx,
               const SerdNodeID             s,
               const SerdNodeID             g)
{
  static const ZixStringView rdf_rest = ZIX_STATIC_STRING(NS_RDF "rest");
  static const ZixStringView rdf_nil  = ZIX_STATIC_STRING(NS_RDF "nil");

  return serd_sink_event(
    ctx->sink,
    serd_statement_event(
      0U,
      serd_quad_view(serd_strings_token(ctx->strings, s),
                     serd_token_view(SERD_URI, rdf_rest),
                     serd_object_view(SERD_URI, rdf_nil, 0U, serd_no_token()),
                     serd_strings_token(ctx->strings, g))));
}

static SerdStatus
write_list(const DescribeContext* const ctx,
           const unsigned               depth,
           SerdEventFlags               flags,
           SerdNodeID                   node_id,
           const SerdNodeID             graph_id)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  SerdCursor f =
    serd_model_find_internal(model, node_id, ctx->rdf_first_id, 0U, graph_id);

  assert(serd_cursor_tuple(&f).nodes[0]);

  while (!st && node_id != ctx->rdf_nil_id) {
    // Write rdf:first statement for this node
    if ((st = write_range_statement(ctx, depth, flags, &f, 0U, false))) {
      return st;
    }

    // Get rdf:rest statement
    const SerdCursor r =
      serd_model_find_internal(model, node_id, ctx->rdf_rest_id, 0U, graph_id);
    const SerdTuple rs = serd_cursor_tuple(&r);

    if (!rs.nodes[0]) {
      // Terminate malformed list with missing rdf:rest
      return write_list_end(ctx, node_id, graph_id);
    }

    // Get rdf:first statement
    const SerdNodeID next_id = rs.nodes[2];
    f =
      serd_model_find_internal(model, next_id, ctx->rdf_first_id, 0U, graph_id);

    // Terminate if the next node has no rdf:first
    if (!serd_cursor_tuple(&f).nodes[0]) {
      return write_list_end(ctx, node_id, graph_id);
    }

    // Write rdf:next statement and move to the next node
    st      = emit_statement(ctx, 0U, &r);
    node_id = next_id;
    flags   = 0U;
  }

  return st;
}

static bool
skip_range_statement(const DescribeContext* const ctx,
                     const NodeStyle              subject_style,
                     const SerdNodeID             predicate_id)
{
  return
    // Skip subject that will be inlined elsewhere
    subject_style == ANON_O || subject_style == LIST_O ||

    // Skip list statement that write_list will handle
    (subject_style == LIST_S &&
     (predicate_id == ctx->rdf_first_id || predicate_id == ctx->rdf_rest_id));
}

static SerdStatus
write_subject_types(const DescribeContext* const ctx,
                    const unsigned               depth,
                    const SerdNodeID             subject_id,
                    const SerdNodeID             graph_id)
{
  SerdCursor t = serd_model_find_internal(
    ctx->model, subject_id, ctx->rdf_type_id, 0U, graph_id);

  return serd_cursor_is_end(&t)
           ? SERD_SUCCESS
           : write_range(ctx, depth + 1, &t, subject_id, true);
}

static bool
types_first_for_subject(const DescribeContext* const ctx, const NodeStyle style)
{
  return style != LIST_S && !(ctx->flags & SERD_DESCRIBE_SORTED_TYPE);
}

static SerdStatus
write_range_statement(const DescribeContext* const ctx,
                      const unsigned               depth,
                      SerdEventFlags               statement_flags,
                      const SerdCursor* const      cursor,
                      const SerdNodeID             last_subject,
                      const bool                   write_types)
{
  const SerdModel* const model         = ctx->model;
  const SerdSink* const  sink          = ctx->sink;
  const SerdTuple        statement     = serd_cursor_tuple(cursor);
  const SerdNodeID       subject_id    = statement.nodes[0];
  const NodeStyle        subject_style = get_node_style(ctx, subject_id);
  const SerdNodeID       predicate_id  = statement.nodes[1];
  const SerdNodeID       object_id     = statement.nodes[2];
  const NodeStyle        object_style  = get_node_style(ctx, object_id);
  const SerdNodeID       graph_id      = statement.nodes[3];
  const NodeStyle        graph_style   = get_node_style(ctx, graph_id);
  SerdStatus             st            = SERD_SUCCESS;

  assert(subject_id);
  assert(predicate_id);
  assert(object_id);

  if (depth == 0U) {
    if (skip_range_statement(ctx, subject_style, predicate_id)) {
      return SERD_SUCCESS; // Skip subject that will be inlined elsewhere
    }

    if (subject_style == LIST_S) {
      // First write inline list subject, which this statement will follow
      const uintptr_t sid = (uintptr_t)subject_id;
      if (zix_hash_insert(ctx->list_subjects, (void*)sid) !=
          ZIX_STATUS_EXISTS) {
        st = write_list(
          ctx, 2, statement_flags | SERD_LIST_S, subject_id, graph_id);
      }
    }
  }

  if (st) {
    return st;
  }

  // If this is a new subject, write types first if necessary
  const bool types_first = types_first_for_subject(ctx, subject_style);
  if (subject_id != last_subject && types_first) {
    st = write_subject_types(ctx, depth, subject_id, graph_id);
  }

  // Skip type statement if it would be written another time (just above)
  if (st || (types_first && predicate_id == ctx->rdf_type_id && !write_types)) {
    return st;
  }

  // Set up the flags for this statement
  statement_flags |=
    (((subject_style == ANON_S) * (SerdEventFlags)SERD_EMPTY_S) |
     ((object_style == EMPTY_O) * (SerdEventFlags)SERD_EMPTY_O) |
     ((object_style == ANON_O) * (SerdEventFlags)SERD_ANON_O) |
     ((object_style == LIST_O) * (SerdEventFlags)SERD_LIST_O) |
     ((graph_style == ANON_G) * (SerdEventFlags)SERD_EMPTY_G));

  // Finally write this statement
  if ((st = emit_statement(ctx, statement_flags, cursor))) {
    return st;
  }

  if (object_style == ANON_O) {
    // Follow an anonymous object with its description like "[ ... ]"
    SerdCursor iter = serd_model_find_internal(model, object_id, 0U, 0U, 0U);

    if (!(st = write_range(ctx, depth + 1, &iter, last_subject, false))) {
      st = serd_sink_event(
        sink,
        serd_end_event(serd_strings_token(ctx->strings, object_id).string));
    }

  } else if (object_style == LIST_O) {
    // Follow a list object with its description like "( ... )"
    st = write_list(ctx, depth + 1, 0U, object_id, graph_id);
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
    zix_hash_new(allocator, identity, id_hash, id_equals);
  if (!list_subjects) {
    return SERD_BAD_ALLOC;
  }

  const SerdNodes* const nodes   = serd_model_nodes(range->model);
  SerdStrings* const     strings = serd_strings_new(allocator, nodes);
  if (!strings) {
    zix_hash_free(list_subjects);
    return SERD_BAD_ALLOC;
  }

  DescribeContext ctx = {
    range->model,
    sink,
    strings,
    list_subjects,
    serd_nodes_find(nodes, serd_a_uri(zix_string(NS_RDF "first"))),
    serd_nodes_find(nodes, serd_a_uri(zix_string(NS_RDF "rest"))),
    serd_nodes_find(nodes, serd_a_uri(zix_string(NS_RDF "type"))),
    serd_nodes_find(nodes, serd_a_uri(zix_string(NS_RDF "nil"))),
    range->strategy.order,
    flags};

  /* We could tolerate these not being present in the model at all by assuming
     that any queries using them match nothing (meaning there's no lists), but
     since they're built in to the nodes storage, assert them up front to avoid
     needing to handle this (currently impossible) scenario. */
  assert(ctx.rdf_first_id);
  assert(ctx.rdf_rest_id);
  assert(ctx.rdf_type_id);
  assert(ctx.rdf_nil_id);

  const SerdStatus st =
    write_range(&ctx, 0, &copy, 0U, (flags & SERD_DESCRIBE_SORTED_TYPE));

  serd_strings_free(ctx.strings);
  zix_hash_free(list_subjects);
  return st;
}
