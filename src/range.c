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

#include "range.h"

#include "iter.h"
#include "model.h"
#include "world.h"

#include "zix/common.h"
#include "zix/digest.h"
#include "zix/hash.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum { NAMED, ANON_S, ANON_O, LIST_S, LIST_O } NodeStyle;

static SerdStatus
write_range_statement(const SerdSink*      sink,
                      const SerdModel*     model,
                      ZixHash*             list_subjects,
                      unsigned             depth,
                      SerdStatementFlags   flags,
                      const SerdStatement* statement);

SerdRange*
serd_range_new(SerdIter* const begin, SerdIter* const end)
{
  SerdRange* range = (SerdRange*)malloc(sizeof(SerdRange));

  range->begin = begin;
  range->end   = end;

  return range;
}

SerdRange*
serd_range_copy(const SerdRange* const range)
{
  SerdRange* copy = (SerdRange*)malloc(sizeof(SerdRange));

  memcpy(copy, range, sizeof(SerdRange));
  copy->begin = serd_iter_copy(range->begin);
  copy->end   = serd_iter_copy(range->end);

  return copy;
}

void
serd_range_free(SerdRange* const range)
{
  if (range) {
    serd_iter_free(range->begin);
    serd_iter_free(range->end);
    free(range);
  }
}

const SerdStatement*
serd_range_front(const SerdRange* const range)
{
  return serd_iter_get(range->begin);
}

bool
serd_range_equals(const SerdRange* const lhs, const SerdRange* const rhs)
{
  return (!lhs && !rhs) ||
         (lhs && rhs && serd_iter_equals(lhs->begin, rhs->begin) &&
          serd_iter_equals(lhs->end, rhs->end));
}

bool
serd_range_next(SerdRange* const range)
{
  return serd_iter_next(range->begin);
}

bool
serd_range_empty(const SerdRange* const range)
{
  return !range || serd_iter_equals(range->begin, range->end);
}

const SerdIter*
serd_range_cbegin(const SerdRange* const range)
{
  return range->begin;
}

const SerdIter*
serd_range_cend(const SerdRange* const range)
{
  return range->end;
}

SerdIter*
serd_range_begin(SerdRange* const range)
{
  return range->begin;
}

SerdIter*
serd_range_end(SerdRange* const range)
{
  return range->end;
}

static NodeStyle
get_node_style(const SerdModel* const model, const SerdNode* const node)
{
  if (serd_node_type(node) != SERD_BLANK) {
    return NAMED; // Non-blank node can't be anonymous
  }

  size_t           n_as_object = 0;
  SerdRange* const range = serd_model_range(model, NULL, NULL, node, NULL);
  for (; !serd_range_empty(range); serd_range_next(range), ++n_as_object) {
    if (n_as_object == 1) {
      serd_range_free(range);
      return NAMED; // Blank node referred to several times
    }
  }

  serd_range_free(range);

  if (serd_model_ask(model, node, model->world->rdf_first, NULL, NULL) &&
      serd_model_ask(model, node, model->world->rdf_rest, NULL, NULL)) {
    return n_as_object == 0 ? LIST_S : LIST_O;
  }

  return n_as_object == 0 ? ANON_S : ANON_O;
}

static uint32_t
ptr_hash(const void* const ptr)
{
  return zix_digest_add_ptr(zix_digest_start(), ptr);
}

static bool
ptr_equals(const void* const a, const void* const b)
{
  return *(const void* const*)a == *(const void* const*)b;
}

static SerdStatus
write_pretty_range(const SerdSink* const    sink,
                   const unsigned           depth,
                   const SerdStatementFlags flags,
                   const SerdModel* const   model,
                   SerdRange* const         range)
{
  (void)flags;

  ZixHash* list_subjects = zix_hash_new(ptr_hash, ptr_equals, sizeof(void*));

  SerdStatus st = SERD_SUCCESS;
  for (; !st && !serd_range_empty(range); serd_range_next(range)) {
    const SerdStatement* const statement = serd_range_front(range);
    assert(statement);

    st = write_range_statement(sink, model, list_subjects, depth, 0, statement);
  }

  zix_hash_free(list_subjects);

  return st;
}

static SerdStatus
write_list(const SerdSink* const  sink,
           const SerdModel* const model,
           ZixHash* const         list_subjects,
           const unsigned         depth,
           SerdStatementFlags     flags,
           const SerdNode*        object)
{
  SerdStatus       st    = SERD_SUCCESS;
  const SerdWorld* world = model->world;
  const SerdNode*  first = world->rdf_first;
  const SerdNode*  rest  = world->rdf_rest;
  const SerdNode*  nil   = world->rdf_nil;

  SerdIter* f = serd_model_find(model, object, first, NULL, NULL);
  while (!st && f && !serd_node_equals(object, nil)) {
    const SerdStatement* const fs = serd_iter_get(f);
    assert(fs);

    st =
      write_range_statement(sink, model, list_subjects, depth + 1, flags, fs);

    serd_iter_free(f);

    f     = NULL;
    flags = 0;
    if (st) {
      break;
    }

    SerdIter* const r = serd_model_find(model, object, rest, NULL, NULL);

    const SerdStatement* const s    = r ? serd_iter_get(r) : NULL;
    const SerdNode*            next = s ? serd_statement_object(s) : NULL;

    f = next ? serd_model_find(model, next, first, NULL, NULL) : NULL;
    if (r && f) {
      // This and next node are good, write rdf:next statement
      st     = serd_sink_write_statement(sink, 0, s);
      object = next;
    } else {
      // Terminate malformed list
      st =
        serd_sink_write(sink, 0, object, rest, nil, serd_statement_graph(fs));
      f = NULL;
    }

    serd_iter_free(r);
  }

  serd_iter_free(f);
  return st;
}

static SerdStatus
write_range_statement(const SerdSink*      sink,
                      const SerdModel*     model,
                      ZixHash*             list_subjects,
                      const unsigned       depth,
                      SerdStatementFlags   flags,
                      const SerdStatement* statement)
{
  const SerdNode* subject       = serd_statement_subject(statement);
  const NodeStyle subject_style = get_node_style(model, subject);

  if (depth == 0 && (subject_style == ANON_O || subject_style == LIST_O)) {
    return SERD_SUCCESS; // Skip subject that will be inlined somewhere
  }

  if (subject_style == LIST_S && depth == 0 &&
      (serd_node_equals(serd_statement_predicate(statement),
                        model->world->rdf_first) ||
       (serd_node_equals(serd_statement_predicate(statement),
                         model->world->rdf_rest)))) {
    return SERD_SUCCESS;
  }

  if (subject_style == ANON_S) {
    flags |= SERD_EMPTY_S; // Write anonymous subjects in "[] p o" style
  }

  const SerdNode* object       = serd_statement_object(statement);
  const NodeStyle object_style = get_node_style(model, object);
  SerdStatus      st           = SERD_SUCCESS;
  if (subject_style == LIST_S && depth == 0) {
    if (zix_hash_insert(list_subjects, &subject, NULL) != ZIX_STATUS_EXISTS) {
      st =
        write_list(sink, model, list_subjects, depth + 1, SERD_LIST_S, subject);
    }
  }

  if (object_style == ANON_O) {
    flags |= SERD_ANON_O;

    SerdRange* const range = serd_model_range(model, object, NULL, NULL, NULL);
    st = st ? st : serd_sink_write_statement(sink, flags, statement);
    st = st ? st : write_pretty_range(sink, depth + 1, 0, model, range);
    st = st ? st : serd_sink_write_end(sink, object);
    serd_range_free(range);

    return st;
  }

  if (object_style == LIST_O) {
    flags |= SERD_LIST_O;
    st = st ? st : serd_sink_write_statement(sink, flags, statement);
    st = st ? st : write_list(sink, model, list_subjects, depth + 1, 0, object);
    return st;
  }

  return serd_sink_write_statement(sink, flags, statement);
}

SerdStatus
serd_write_range(const SerdRange* const       range,
                 const SerdSink*              sink,
                 const SerdSerialisationFlags flags)
{
  SerdStatus st = SERD_SUCCESS;

  if (!serd_range_empty(range)) {
    SerdRange* const copy = serd_range_copy(range);
    if (flags & SERD_NO_INLINE_OBJECTS) {
      for (; !st && !serd_range_empty(copy); serd_range_next(copy)) {
        const SerdStatement* const f = serd_range_front(copy);
        st = f ? serd_sink_write_statement(sink, 0, f) : SERD_ERR_INTERNAL;
      }
    } else {
      st = write_pretty_range(sink, 0, 0, range->begin->model, copy);
    }

    serd_range_free(copy);
  }

  return st;
}
