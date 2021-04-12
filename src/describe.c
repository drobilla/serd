// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "cursor.h"
#include "model.h"
#include "world.h"

// Define the types used in the hash interface for more type safety
#define ZIX_HASH_KEY_TYPE const SerdNode
#define ZIX_HASH_RECORD_TYPE const SerdNode

#include "serd/attributes.h"
#include "serd/cursor.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/range.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/digest.h"
#include "zix/hash.h"
#include "zix/status.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum { NAMED, ANON_S, ANON_O, LIST_S, LIST_O } NodeStyle;

static SerdStatus
write_range_statement(const SerdSink*      sink,
                      const SerdModel*     model,
                      ZixHash*             list_subjects,
                      unsigned             depth,
                      SerdStatementFlags   flags,
                      const SerdStatement* statement);

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
write_pretty_range(const SerdSink* const  sink,
                   const unsigned         depth,
                   const SerdModel* const model,
                   SerdCursor* const      range)
{
  ZixHash* const list_subjects =
    zix_hash_new(NULL, identity, ptr_hash, ptr_equals);

  SerdStatus st = SERD_SUCCESS;

  while (!st && !serd_cursor_is_end(range)) {
    const SerdStatement* const statement = serd_cursor_get(range);
    assert(statement);

    if (!(st = write_range_statement(
            sink, model, list_subjects, depth, 0, statement))) {
      st = serd_cursor_advance(range);
    }
  }

  zix_hash_free(list_subjects);

  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
write_list(const SerdSink* const  sink,
           const SerdModel* const model,
           ZixHash* const         list_subjects,
           const unsigned         depth,
           SerdStatementFlags     flags,
           const SerdNode*        object,
           const SerdNode* const  graph)
{
  const SerdWorld* const world     = model->world;
  const SerdNode* const  rdf_first = world->rdf_first;
  const SerdNode* const  rdf_rest  = world->rdf_rest;
  const SerdNode* const  rdf_nil   = world->rdf_nil;
  SerdStatus             st        = SERD_SUCCESS;

  const SerdStatement* fs =
    serd_model_get_statement(model, object, rdf_first, NULL, graph);

  assert(fs); // Shouldn't get here if it doesn't at least have an rdf:first

  while (!st && !serd_node_equals(object, rdf_nil)) {
    // Write rdf:first statement for this node
    if ((st = write_range_statement(
           sink, model, list_subjects, depth, flags, fs))) {
      return st;
    }

    // Get rdf:rest statement
    const SerdStatement* const rs =
      serd_model_get_statement(model, object, rdf_rest, NULL, graph);

    if (!rs) {
      // Terminate malformed list with missing rdf:rest
      return serd_sink_write(sink, 0, object, rdf_rest, rdf_nil, graph);
    }

    // Terminate if the next node has no rdf:first
    const SerdNode* const next = serd_statement_object(rs);
    if (!(fs = serd_model_get_statement(model, next, rdf_first, NULL, graph))) {
      return serd_sink_write(sink, 0, object, rdf_rest, rdf_nil, graph);
    }

    // Write rdf:next statement and move to the next node
    st     = serd_sink_write_statement(sink, 0, rs);
    object = next;
    flags  = 0U;
  }

  return st;
}

static bool
skip_range_statement(const SerdModel* const     model,
                     const SerdStatement* const statement)
{
  const SerdNode* const subject       = serd_statement_subject(statement);
  const NodeStyle       subject_style = get_node_style(model, subject);
  const SerdNode* const predicate     = serd_statement_predicate(statement);

  if (subject_style == ANON_O || subject_style == LIST_O) {
    return true; // Skip subject that will be inlined elsewhere
  }

  if (subject_style == LIST_S &&
      (serd_node_equals(predicate, model->world->rdf_first) ||
       serd_node_equals(predicate, model->world->rdf_rest))) {
    return true; // Skip list statement that write_list will handle
  }

  return false;
}

static SerdStatus
write_range_statement(const SerdSink* const             sink,
                      const SerdModel* const            model,
                      ZixHash* const                    list_subjects,
                      const unsigned                    depth,
                      SerdStatementFlags                flags,
                      const SerdStatement* SERD_NONNULL statement)
{
  const SerdNode* const subject       = serd_statement_subject(statement);
  const NodeStyle       subject_style = get_node_style(model, subject);
  const SerdNode* const object        = serd_statement_object(statement);
  const NodeStyle       object_style  = get_node_style(model, object);
  const SerdNode* const graph         = serd_statement_graph(statement);
  SerdStatus            st            = SERD_SUCCESS;

  if (subject_style == ANON_S) { // Write anonymous subject like "[] p o"
    flags |= SERD_EMPTY_S;
  }

  if (depth == 0U) {
    if (skip_range_statement(model, statement)) {
      return SERD_SUCCESS; // Skip subject that will be inlined elsewhere
    }

    if (subject_style == LIST_S) {
      // First write inline list subject, which this statement will follow
      if (zix_hash_insert(list_subjects, subject) != ZIX_STATUS_EXISTS) {
        st = write_list(
          sink, model, list_subjects, 2, SERD_LIST_S, subject, graph);
      }
    }
  }

  if (st) {
    return st;
  }

  if (object_style == ANON_O) { // Write anonymous object like "[ ... ]"
    SerdCursor* const iter = serd_model_find(model, object, NULL, NULL, NULL);

    flags |= SERD_ANON_O;
    if (!(st = serd_sink_write_statement(sink, flags, statement))) {
      if (!(st = write_pretty_range(sink, depth + 1, model, iter))) {
        st = serd_sink_write_end(sink, object);
      }
    }

    serd_cursor_free(iter);

  } else if (object_style == LIST_O) { // Write list object like "( ... )"
    flags |= SERD_LIST_O;
    if (!(st = serd_sink_write_statement(sink, flags, statement))) {
      st = write_list(sink, model, list_subjects, depth + 1, 0, object, graph);
    }

  } else {
    st = serd_sink_write_statement(sink, flags, statement);
  }

  return st;
}

SerdStatus
serd_describe_range(const SerdCursor* const range,
                    const SerdSink*         sink,
                    const SerdDescribeFlags flags)
{
  assert(sink);

  SerdStatus st = SERD_SUCCESS;

  if (serd_cursor_is_end(range)) {
    return st;
  }

  SerdCursor copy = *range;

  if (flags & SERD_NO_INLINE_OBJECTS) {
    const SerdStatement* statement = NULL;
    while (!st && (statement = serd_cursor_get(&copy))) {
      if (!(st = serd_sink_write_statement(sink, 0, statement))) {
        st = serd_cursor_advance(&copy);
      }
    }
  } else {
    st = write_pretty_range(sink, 0, range->model, &copy);
  }

  return st;
}
