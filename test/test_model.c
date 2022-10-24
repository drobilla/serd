// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/caret_view.h>
#include <serd/cursor.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/field.h>
#include <serd/handler.h>
#include <serd/inserter.h>
#include <serd/log.h>
#include <serd/model.h>
#include <serd/node_args.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_EG "http://example.org/"

enum { N_OBJECTS_PER = 2U };

typedef SerdNodeID Quad[4];

typedef struct {
  Quad     query;
  unsigned expected_num_results;
} QueryTest;

static SerdNodeID
uri(SerdNodes* const nodes, const unsigned num)
{
  char str[] = "http://example.org/0000000000";

  snprintf(str + 19, 11, "%010u", num);

  return serd_nodes_id(nodes, serd_a_uri(zix_string(str)));
}

static SerdStatus
add_from(SerdModel* const       model,
         const SerdNodes* const nodes,
         const SerdNodeID       s,
         const SerdNodeID       p,
         const SerdNodeID       o,
         const SerdNodeID       g,
         const SerdCaretView    caret)
{
  return serd_model_add_from(
    model,
    serd_a_token_view(serd_nodes_get_token(nodes, s)),
    serd_a_token_view(serd_nodes_get_token(nodes, p)),
    serd_a_object_view(serd_nodes_get_object(nodes, o)),
    serd_a_token_view(serd_nodes_get_token(nodes, g)),
    caret);
}

static SerdStatus
add(SerdModel* const       model,
    const SerdNodes* const nodes,
    const SerdNodeID       s,
    const SerdNodeID       p,
    const SerdNodeID       o,
    const SerdNodeID       g)
{
  return serd_model_insert(model,
                           serd_quad_view(serd_nodes_get_token(nodes, s),
                                          serd_nodes_get_token(nodes, p),
                                          serd_nodes_get_object(nodes, o),
                                          serd_nodes_get_token(nodes, g)));
}

static SerdNodeID
get(const SerdModel* const model,
    const SerdNodes* const nodes,
    const SerdNodeID       s,
    const SerdNodeID       p,
    const SerdNodeID       o,
    const SerdNodeID       g)
{
  return serd_model_get(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, s)),
                        serd_a_token_view(serd_nodes_get_token(nodes, p)),
                        serd_a_object_view(serd_nodes_get_object(nodes, o)),
                        serd_a_token_view(serd_nodes_get_token(nodes, g)));
}

static bool
ask(const SerdModel* const model,
    const SerdNodes* const nodes,
    const SerdNodeID       s,
    const SerdNodeID       p,
    const SerdNodeID       o,
    const SerdNodeID       g)
{
  const SerdTokenView  sv = serd_nodes_get_token(nodes, s);
  const SerdTokenView  pv = serd_nodes_get_token(nodes, p);
  const SerdObjectView ov = serd_nodes_get_object(nodes, o);
  const SerdTokenView  gv = serd_nodes_get_token(nodes, g);

  const bool result = serd_model_ask(model,
                                     serd_a_token_view(sv),
                                     serd_a_token_view(pv),
                                     serd_a_object_view(ov),
                                     serd_a_token_view(gv));

  if (result) {
    const SerdStatementView statement =
      serd_model_get_statement(model,
                               serd_a_token_view(sv),
                               serd_a_token_view(pv),
                               serd_a_object_view(ov),
                               serd_a_token_view(gv));
    assert(!s || serd_token_view_equals(statement.subject, sv));
    assert(!p || serd_token_view_equals(statement.predicate, pv));
    assert(!o || serd_object_view_equals(statement.object, ov));
    assert(!g || serd_token_view_equals(statement.graph, gv));
  }

  return result;
}

static int
generate(SerdNodes* const nodes,
         SerdModel* const model,
         const size_t     n_quads,
         const SerdNodeID graph)
{
  SerdStatus st = SERD_SUCCESS;

  for (unsigned i = 0; i < n_quads; ++i) {
    unsigned num = (i * N_OBJECTS_PER) + 1U;

    SerdNodeID ids[2 + N_OBJECTS_PER];
    for (unsigned j = 0; j < 2 + N_OBJECTS_PER; ++j) {
      ids[j] = uri(nodes, num++);
    }

    for (unsigned j = 0; j < N_OBJECTS_PER; ++j) {
      st = add(model, nodes, ids[0], ids[1], ids[2 + j], graph);
      assert(!st);
    }
  }

  // Add some literals

  // (98 4 "hello") and (98 4 "hello"^^<5>)
  const SerdNodeID hello =
    serd_nodes_id(nodes, serd_a_string(zix_string("hello")));

  const SerdNodeID hello_gb = serd_nodes_id(
    nodes, serd_a_plain_literal(zix_string("hello"), zix_string("en-gb")));

  const SerdNodeID hello_us = serd_nodes_id(
    nodes, serd_a_plain_literal(zix_string("hello"), zix_string("en-us")));

  const SerdNodeID hello_t4 = serd_nodes_id(
    nodes,
    serd_a_typed_literal(zix_string("hello"),
                         zix_string("http://example.org/0000000004")));

  const SerdNodeID hello_t5 = serd_nodes_id(
    nodes,
    serd_a_typed_literal(zix_string("hello"),
                         zix_string("http://example.org/0000000005")));

  assert(!add(model, nodes, uri(nodes, 98), uri(nodes, 4), hello, graph));
  assert(!add(model, nodes, uri(nodes, 98), uri(nodes, 4), hello_t5, graph));

  // (96 4 "hello"^^<4>) and (96 4 "hello"^^<5>)
  assert(!add(model, nodes, uri(nodes, 96), uri(nodes, 4), hello_t4, graph));
  assert(!add(model, nodes, uri(nodes, 96), uri(nodes, 4), hello_t5, graph));

  // (94 5 "hello") and (94 5 "hello"@en-gb)
  assert(!add(model, nodes, uri(nodes, 94), uri(nodes, 5), hello, graph));
  assert(!add(model, nodes, uri(nodes, 94), uri(nodes, 5), hello_gb, graph));

  // (92 6 "hello"@en-us) and (92 6 "hello"@en-gb)
  assert(!add(model, nodes, uri(nodes, 92), uri(nodes, 6), hello_us, graph));
  assert(!add(model, nodes, uri(nodes, 92), uri(nodes, 6), hello_gb, graph));

  // (14 6 "bonjour"@fr) and (14 6 "salut"@fr)

  const SerdNodeID bonjour = serd_nodes_id(
    nodes, serd_a_plain_literal(zix_string("bonjour"), zix_string("fr")));

  const SerdNodeID salut = serd_nodes_id(
    nodes, serd_a_plain_literal(zix_string("salut"), zix_string("fr")));

  assert(!add(model, nodes, uri(nodes, 14), uri(nodes, 6), bonjour, graph));
  assert(!add(model, nodes, uri(nodes, 14), uri(nodes, 6), salut, graph));

  // Attempt to add duplicates
  assert(add(model, nodes, uri(nodes, 14), uri(nodes, 6), salut, graph));

  // Add a blank node subject
  const SerdNodeID ablank =
    serd_nodes_id(nodes, serd_a_blank(zix_string("ablank")));

  assert(!add(model, nodes, ablank, uri(nodes, 6), salut, graph));

  // Add statement with URI object
  assert(!add(model, nodes, ablank, uri(nodes, 6), uri(nodes, 7), graph));

  return EXIT_SUCCESS;
}

static bool
token_view_matches(const SerdNodes* const nodes,
                   const SerdTokenView    a,
                   const SerdNodeID       b_id)
{
  return !b_id || serd_token_view_equals(a, serd_nodes_get_token(nodes, b_id));
}

static bool
object_view_matches(const SerdNodes* const nodes,
                    const SerdObjectView   a,
                    const SerdNodeID       b_id)
{
  return !b_id ||
         serd_object_view_equals(a, serd_nodes_get_object(nodes, b_id));
}

ZIX_PURE_FUNC static bool
node_ids_equal(const SerdNodes* const a_nodes,
               const SerdNodeID       a,
               const SerdNodes* const b_nodes,
               const SerdNodeID       b)
{
  assert(a);
  assert(b);
  return serd_object_view_equals(serd_nodes_get_object(a_nodes, a),
                                 serd_nodes_get_object(b_nodes, b));
}

static bool
statement_view_matches(const SerdStatementView statement,
                       const SerdNodes* const  nodes,
                       const SerdNodeID        subject,
                       const SerdNodeID        predicate,
                       const SerdNodeID        object,
                       const SerdNodeID        graph)
{
  return (!subject ||
          serd_token_view_equals(statement.subject,
                                 serd_nodes_get_token(nodes, subject))) &&
         (!predicate ||
          serd_token_view_equals(statement.predicate,
                                 serd_nodes_get_token(nodes, predicate))) &&
         (!object ||
          serd_object_view_equals(statement.object,
                                  serd_nodes_get_object(nodes, object))) &&
         (!graph || serd_token_view_equals(statement.graph,
                                           serd_nodes_get_token(nodes, graph)));
}

static int
test_read(const SerdModel* const model,
          SerdNodes* const       nodes,
          const SerdNodeID       g,
          const unsigned         n_quads)
{
  SerdCursor* const cursor = serd_model_begin(NULL, model);
  SerdStatementView prev   = {
    serd_no_token(), serd_no_token(), serd_no_object(), serd_no_token()};

  for (; !serd_cursor_equals(cursor, serd_model_end(model));
       serd_cursor_advance(cursor)) {
    const SerdStatementView statement = serd_cursor_get(cursor);
    assert(serd_field_supports(SERD_SUBJECT, statement.subject.type));
    assert(serd_field_supports(SERD_PREDICATE, statement.predicate.type));
    assert(serd_field_supports(SERD_OBJECT, statement.object.type));
    assert(statement.graph.type == SERD_NOTHING ||
           serd_field_supports(SERD_GRAPH, statement.graph.type));
    assert(!serd_statement_view_equals(statement, prev));
    assert(!serd_statement_view_equals(prev, statement));
    prev = statement;
  }

  // Attempt to increment past end
  assert(serd_cursor_advance(cursor) == SERD_BAD_CURSOR);
  serd_cursor_free(NULL, cursor);

  static const ZixStringView s = ZIX_STATIC_STRING("hello");

  const SerdNodeID plain_hello = serd_nodes_id(nodes, serd_a_string(s));

  const SerdNodeID type4_hello = serd_nodes_id(
    nodes,
    serd_a_typed_literal(s, zix_string("http://example.org/0000000004")));

  const SerdNodeID type5_hello = serd_nodes_id(
    nodes,
    serd_a_typed_literal(s, zix_string("http://example.org/0000000005")));

  const SerdNodeID gb_hello =
    serd_nodes_id(nodes, serd_a_plain_literal(s, zix_string("en-gb")));

  const SerdNodeID us_hello =
    serd_nodes_id(nodes, serd_a_plain_literal(s, zix_string("en-us")));

#define WILDCARD_NODE 0U
#define NUM_PATTERNS 18

  QueryTest patterns[NUM_PATTERNS] = {
    {{0U, 0U, 0U}, (n_quads * N_OBJECTS_PER) + 12U},
    {{uri(nodes, 1), WILDCARD_NODE, WILDCARD_NODE}, 2},
    {{uri(nodes, 9), uri(nodes, 9), uri(nodes, 9)}, 0},
    {{uri(nodes, 1), uri(nodes, 2), uri(nodes, 4)}, 1},
    {{uri(nodes, 3), uri(nodes, 4), WILDCARD_NODE}, 2},
    {{WILDCARD_NODE, uri(nodes, 2), uri(nodes, 4)}, 1},
    {{WILDCARD_NODE, WILDCARD_NODE, uri(nodes, 4)}, 1},
    {{uri(nodes, 1), WILDCARD_NODE, WILDCARD_NODE}, 2},
    {{uri(nodes, 1), WILDCARD_NODE, uri(nodes, 4)}, 1},
    {{WILDCARD_NODE, uri(nodes, 2), WILDCARD_NODE}, 2},
    {{uri(nodes, 98), uri(nodes, 4), plain_hello}, 1},
    {{uri(nodes, 98), uri(nodes, 4), type5_hello}, 1},
    {{uri(nodes, 96), uri(nodes, 4), type4_hello}, 1},
    {{uri(nodes, 96), uri(nodes, 4), type5_hello}, 1},
    {{uri(nodes, 94), uri(nodes, 5), plain_hello}, 1},
    {{uri(nodes, 94), uri(nodes, 5), gb_hello}, 1},
    {{uri(nodes, 92), uri(nodes, 6), gb_hello}, 1},
    {{uri(nodes, 92), uri(nodes, 6), us_hello}, 1}};

  const Quad match = {uri(nodes, 1), uri(nodes, 2), uri(nodes, 4), g};
  assert(
    serd_model_ask(model,
                   serd_a_token_view(serd_nodes_get_token(nodes, match[0])),
                   serd_a_token_view(serd_nodes_get_token(nodes, match[1])),
                   serd_a_object_view(serd_nodes_get_object(nodes, match[2])),
                   serd_a_token_view(serd_nodes_get_token(nodes, match[3]))));

  const Quad nomatch = {uri(nodes, 1), uri(nodes, 2), uri(nodes, 9), g};
  assert(!serd_model_ask(
    model,
    serd_a_token_view(serd_nodes_get_token(nodes, nomatch[0])),
    serd_a_token_view(serd_nodes_get_token(nodes, nomatch[1])),
    serd_a_object_view(serd_nodes_get_object(nodes, nomatch[2])),
    serd_a_token_view(serd_nodes_get_token(nodes, nomatch[3]))));

  assert(!get(model, nodes, 0U, 0U, uri(nodes, 3), g));
  assert(!get(model, nodes, uri(nodes, 1), uri(nodes, 99), 0U, g));

  assert(node_ids_equal(serd_model_nodes(model),
                        get(model, nodes, uri(nodes, 1), uri(nodes, 2), 0U, g),
                        nodes,
                        uri(nodes, 3)));
  assert(node_ids_equal(serd_model_nodes(model),
                        get(model, nodes, uri(nodes, 1), 0U, uri(nodes, 3), g),
                        nodes,
                        uri(nodes, 2)));
  assert(node_ids_equal(serd_model_nodes(model),
                        get(model, nodes, 0U, uri(nodes, 2), uri(nodes, 3), g),
                        nodes,
                        uri(nodes, 1)));
  if (g) {
    assert(node_ids_equal(
      serd_model_nodes(model),
      get(model, nodes, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0U),
      nodes,
      g));
  }

  for (unsigned i = 0; i < NUM_PATTERNS; ++i) {
    QueryTest  test = patterns[i];
    const Quad pat  = {test.query[0], test.query[1], test.query[2], g};

    SerdCursor* const range =
      serd_model_find(NULL,
                      model,
                      serd_a_token_view(serd_nodes_get_token(nodes, pat[0])),
                      serd_a_token_view(serd_nodes_get_token(nodes, pat[1])),
                      serd_a_object_view(serd_nodes_get_object(nodes, pat[2])),
                      serd_a_token_view(serd_nodes_get_token(nodes, pat[3])));

    unsigned num_results = 0U;
    for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
      ++num_results;

      const SerdStatementView first = serd_cursor_get(range);
      assert(first.subject.type);
      assert(first.predicate.type);
      assert(first.object.type);
      assert(token_view_matches(nodes, first.subject, pat[0]));
      assert(token_view_matches(nodes, first.predicate, pat[1]));
      assert(object_view_matches(nodes, first.object, pat[2]));
      assert(token_view_matches(nodes, first.graph, pat[3]));
    }

    serd_cursor_free(NULL, range);

    assert(num_results == test.expected_num_results);
  }

#undef NUM_PATTERNS

  // Query blank node subject

  const SerdNodeID ablank =
    serd_nodes_id(nodes, serd_a_blank(zix_string("ablank")));

  const Quad  pat         = {ablank, 0, 0};
  unsigned    num_results = 0U;
  SerdCursor* range =
    serd_model_find(NULL,
                    model,
                    serd_a_token_view(serd_nodes_get_token(nodes, pat[0])),
                    serd_a_token_view(serd_nodes_get_token(nodes, pat[1])),
                    serd_a_token_view(serd_nodes_get_token(nodes, pat[2])),
                    serd_a_token_view(serd_nodes_get_token(nodes, pat[3])));

  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    ++num_results;
    const SerdStatementView statement = serd_cursor_get(range);
    assert(statement.subject.type);
    assert(statement.predicate.type);
    assert(statement.object.type);
    assert(token_view_matches(nodes, statement.subject, pat[0]));
    assert(token_view_matches(nodes, statement.predicate, pat[1]));
    assert(object_view_matches(nodes, statement.object, pat[2]));
    assert(token_view_matches(nodes, statement.graph, pat[3]));
  }
  serd_cursor_free(NULL, range);

  assert(num_results == 2U);

  // Test nested queries
  SerdTokenView last_subject = {SERD_LITERAL, {"", 0U}};

  range = serd_model_find(
    NULL, model, serd_a_null(), serd_a_null(), serd_a_null(), serd_a_null());
  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    const SerdStatementView statement = serd_cursor_get(range);
    const SerdTokenView     subject   = statement.subject;
    if (serd_token_view_equals(subject, last_subject)) {
      continue;
    }

    // FIXME: blech
    const Quad subpat = {
      serd_nodes_id(nodes, serd_a_token_view(subject)), 0, 0};
    SerdCursor* const subrange = serd_model_find(
      NULL,
      model,
      serd_a_token_view(serd_nodes_get_token(nodes, subpat[0])),
      serd_a_token_view(serd_nodes_get_token(nodes, subpat[1])),
      serd_a_token_view(serd_nodes_get_token(nodes, subpat[2])),
      serd_a_token_view(serd_nodes_get_token(nodes, subpat[3])));

    assert(subrange);

    const SerdStatementView substatement    = serd_cursor_get(subrange);
    uint64_t                num_sub_results = 0;
    assert(serd_token_view_equals(substatement.subject, subject));
    for (; !serd_cursor_is_end(subrange); serd_cursor_advance(subrange)) {
      const SerdStatementView front = serd_cursor_get(subrange);

      assert(statement_view_matches(
        front, nodes, subpat[0], subpat[1], subpat[2], subpat[3]));

      ++num_sub_results;
    }
    serd_cursor_free(NULL, subrange);
    assert(num_sub_results == N_OBJECTS_PER);

    // FIXME: blech
    const uint64_t count = serd_model_count(model,
                                            serd_a_token_view(subject),
                                            serd_a_null(),
                                            serd_a_null(),
                                            serd_a_null());
    assert(count == num_sub_results);

    last_subject = subject;
  }
  serd_cursor_free(NULL, range);

  return 0;
}

static SerdStatus
expected_error(void* const         handle,
               const SerdLogLevel  level,
               const SerdCaretView caret,
               const ZixStringView message)
{
  (void)level;
  (void)caret;
  (void)handle;

  fprintf(stderr, "expected: %s\n", message.data);
  return SERD_SUCCESS;
}

ZIX_PURE_FUNC static SerdStatus
ignore_only_index_error(void* const         handle,
                        const SerdLogLevel  level,
                        const SerdCaretView caret,
                        const ZixStringView message)
{
  (void)handle;
  (void)level;
  (void)caret;

  const bool is_index_error = strstr(message.data, "index");

  assert(is_index_error);

  return is_index_error ? SERD_SUCCESS : SERD_UNKNOWN_ERROR;
}

static int
test_free_null(SerdWorld* const world, const unsigned n_quads)
{
  (void)world;
  (void)n_quads;

  serd_model_free(NULL); // Shouldn't crash
  return 0;
}

static int
test_get_world(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(model);
  assert(serd_model_world(model) == world);
  serd_model_free(model);
  return 0;
}

static int
test_get_default_order(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdModel* const model1 = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(model1);

  SerdModel* const model2 =
    serd_model_new(world, SERD_ORDER_GPSO, SERD_MODEL_GRAPHS);
  assert(model2);

  assert(serd_model_default_order(model1) == SERD_ORDER_SPO);
  assert(serd_model_default_order(model2) == SERD_ORDER_GPSO);

  serd_model_free(model2);
  serd_model_free(model1);
  return 0;
}

static int
test_get_flags(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  const SerdModelFlags flags = SERD_MODEL_GRAPHS | SERD_MODEL_CARETS;
  SerdModel* const     model = serd_model_new(world, SERD_ORDER_SPO, flags);
  assert(model);

  assert(serd_model_flags(model) & SERD_MODEL_GRAPHS);
  assert(serd_model_flags(model) & SERD_MODEL_CARETS);
  serd_model_free(model);
  return 0;
}

static int
test_all_begin(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(model);

  SerdCursor* const begin = serd_model_begin(NULL, model);
  SerdCursor* const first = serd_model_find(
    NULL, model, serd_a_null(), serd_a_null(), serd_a_null(), serd_a_null());

  assert(serd_cursor_equals(begin, first));

  serd_cursor_free(NULL, first);
  serd_cursor_free(NULL, begin);
  serd_model_free(model);
  return 0;
}

static int
test_begin_ordered(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

  assert(model);
  assert(!add(model, nodes, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0U));

  {
    SerdCursor* const i = serd_model_begin_ordered(NULL, model, SERD_ORDER_SPO);
    assert(i);
    assert(!serd_cursor_is_end(i));
    serd_cursor_free(NULL, i);
  }

  {
    SerdCursor* const i = serd_model_begin_ordered(NULL, model, SERD_ORDER_POS);
    assert(serd_cursor_is_end(i));
    serd_cursor_free(NULL, i);
  }

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_with_iterator(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(model);

  serd_world_set_log_func(world, expected_error, NULL);
  assert(!add(model, nodes, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0U));

  // Add a statement with an active iterator
  SerdCursor* iter = serd_model_begin(NULL, model);
  assert(!add(model, nodes, uri(nodes, 1), uri(nodes, 2), uri(nodes, 4), 0U));

  // Check that iterator has been invalidated
  assert(!serd_cursor_get(iter).subject.type);
  assert(!serd_cursor_get(iter).predicate.type);
  assert(!serd_cursor_get(iter).object.type);
  assert(!serd_cursor_get(iter).graph.type);
  assert(serd_cursor_advance(iter) == SERD_BAD_CURSOR);

  serd_cursor_free(NULL, iter);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_remove_nodes(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  assert(model);
  assert(serd_model_nodes(model));
  // assert(serd_nodes_size(serd_model_nodes(model)) == 0);

  const SerdNodeID a = uri(nodes, 1);
  const SerdNodeID b = uri(nodes, 2);
  const SerdNodeID c = uri(nodes, 3);

  // Add 2 statements with 3 nodes
  assert(!add(model, nodes, a, b, a, 0U));
  assert(!add(model, nodes, c, b, c, 0U));
  assert(serd_model_size(model) == 2);
  //  assert(serd_nodes_size(serd_model_nodes(model)) == 3);

  // Remove one statement to leave 2 nodes
  SerdCursor* const begin = serd_model_begin(NULL, model);
  assert(!serd_model_erase(model, begin));
  assert(serd_model_size(model) == 1);
  // FIXME
  // assert(serd_nodes_size(serd_model_nodes(model)) == 2);
  serd_cursor_free(NULL, begin);

  // Clear the last statement to leave 0 nodes
  assert(!serd_model_clear(model));
  // FIXME
  // assert(serd_nodes_size(serd_model_nodes(model)) == 0);

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_index(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(model);

  const SerdNodeID s  = uri(nodes, 0);
  const SerdNodeID p  = uri(nodes, 1);
  const SerdNodeID o1 = uri(nodes, 2);
  const SerdNodeID o2 = uri(nodes, 3);

  // Try to add an existing index
  assert(serd_model_has_index(model, SERD_ORDER_SPO));
  assert(serd_model_add_index(model, SERD_ORDER_SPO) == SERD_FAILURE);

  // Try to add a graph index on a model without graphs
  assert(serd_model_add_index(model, SERD_ORDER_GSPO) == SERD_BAD_ARG);
  assert(!serd_model_has_index(model, SERD_ORDER_GSPO));

  // Add a couple of statements
  assert(!add(model, nodes, s, p, o1, 0U));
  assert(!add(model, nodes, s, p, o2, 0U));
  assert(serd_model_size(model) == 2);

  // Add a new index
  assert(!serd_model_has_index(model, SERD_ORDER_PSO));
  assert(!serd_model_add_index(model, SERD_ORDER_PSO));
  assert(serd_model_has_index(model, SERD_ORDER_PSO));

  // Count statements via the new index
  size_t            count = 0U;
  SerdCursor* const cur =
    serd_model_find(NULL,
                    model,
                    serd_a_null(),
                    serd_a_token_view(serd_nodes_get_token(nodes, p)),
                    serd_a_null(),
                    serd_a_null());
  while (!serd_cursor_is_end(cur)) {
    ++count;
    serd_cursor_advance(cur);
  }
  serd_cursor_free(NULL, cur);

  serd_model_free(model);
  serd_nodes_free(nodes);
  assert(count == 2);
  return 0;
}

static int
test_remove_index(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(model);

  const SerdNodeID s  = uri(nodes, 0);
  const SerdNodeID p  = uri(nodes, 1);
  const SerdNodeID o1 = uri(nodes, 2);
  const SerdNodeID o2 = uri(nodes, 3);

  // Try to remove default and non-existent indices
  assert(serd_model_drop_index(model, SERD_ORDER_SPO) == SERD_BAD_CALL);
  assert(serd_model_drop_index(model, SERD_ORDER_PSO) == SERD_FAILURE);

  // Add a couple of statements so that dropping an index isn't trivial
  assert(!add(model, nodes, s, p, o1, 0U));
  assert(!add(model, nodes, s, p, o2, 0U));
  assert(serd_model_size(model) == 2);

  assert(serd_model_add_index(model, SERD_ORDER_PSO) == SERD_SUCCESS);
  assert(serd_model_drop_index(model, SERD_ORDER_PSO) == SERD_SUCCESS);
  assert(serd_model_drop_index(model, SERD_ORDER_PSO) == SERD_FAILURE);
  assert(serd_model_size(model) == 2);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_inserter(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  static const SerdTokenView s = {SERD_URI, ZIX_STATIC_STRING(NS_EG "s")};
  static const SerdTokenView p = {SERD_URI, ZIX_STATIC_STRING(NS_EG "p")};
  static const SerdTokenView g = {SERD_URI, ZIX_STATIC_STRING(NS_EG "g")};

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());
  assert(env);

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);
  assert(model);

  assert(!serd_inserter_new(
    model, env, serd_token_view(SERD_LITERAL, zix_string("l"))));

  SerdHandler* const inserter = serd_inserter_new(model, env, g);

  serd_world_set_log_func(world, expected_error, NULL);

  // Try to insert a statement with an invalid object

  static const SerdObjectView bad = {SERD_URI,
                                     ZIX_STATIC_STRING("rel"),
                                     0U,
                                     {SERD_LITERAL, ZIX_STATIC_STRING("")}};

  const SerdStatus st1 =
    serd_sink_event(serd_handler_sink(inserter),
                    serd_statement_event(0U, serd_triple_view(s, p, bad)));

  assert(st1 == SERD_BAD_DATA);
  assert(!serd_model_size(model));

  // Insert a valid triple and check that the default graph is used

  static const SerdObjectView o = {SERD_LITERAL,
                                   ZIX_STATIC_STRING("string"),
                                   0U,
                                   {SERD_NOTHING, ZIX_STATIC_STRING("")}};

  const SerdStatus st2 =
    serd_sink_event(serd_handler_sink(inserter),
                    serd_statement_event(0U, serd_triple_view(s, p, o)));

  assert(!st2);
  assert(serd_model_size(model) == 1U);

  SerdCursor* const       iter  = serd_model_begin(NULL, model);
  const SerdStatementView first = serd_cursor_get(iter);
  assert(serd_token_view_equals(first.subject, s));
  assert(serd_token_view_equals(first.predicate, p));
  assert(serd_object_view_equals(first.object, o));
  assert(serd_token_view_equals(first.graph, g));
  serd_cursor_free(NULL, iter);

  serd_handler_free(inserter);
  serd_model_free(model);
  serd_env_free(env);
  return 0;
}

static SerdStatus
check_insert_alloc(ZixAllocator* const allocator)
{
  static const SerdTokenView s = {SERD_URI, ZIX_STATIC_STRING(NS_EG "s")};
  static const SerdTokenView p = {SERD_URI, ZIX_STATIC_STRING(NS_EG "p")};

  char           o_buf[32] = {'\0'};
  SerdObjectView o         = {
    SERD_URI, zix_string(o_buf), 0U, {SERD_LITERAL, {"", 0U}}};

  SerdStatus st = SERD_SUCCESS;

  const SerdCaretView caret = {ZIX_STATIC_STRING("doc"), 1, 1};

  const SerdTokenView default_graph =
    serd_token_view(SERD_URI, zix_string(NS_EG "g"));

  SerdWorld* const world = serd_world_new(allocator);
  if (!world) {
    return SERD_BAD_ALLOC;
  }

  SerdEnv* const   env = serd_env_new(NULL, zix_empty_string());
  SerdModel* const model =
    env ? serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_CARETS) : NULL;

  st = model ? serd_model_add_index(model, SERD_ORDER_OSP) : SERD_BAD_ALLOC;

  if (!st) {
    SerdHandler* const inserter = serd_inserter_new(model, env, default_graph);
    if (!inserter) {
      st = SERD_BAD_ALLOC;
    } else {
      for (unsigned i = 0U; i < 4U; ++i) {
        snprintf(o_buf, sizeof(o_buf), "http://example.org/%u", i);
        o.string = zix_string(o_buf);

        const SerdEvent event = serd_cite_event(
          serd_statement_event(0U, serd_triple_view(s, p, o)), caret);

        st = serd_sink_event(serd_handler_sink(inserter), event);
      }
      serd_handler_free(inserter);
    }
  }

  serd_model_free(model);
  serd_env_free(env);
  serd_world_free(world);
  return st;
}

static int
test_insert_failed_alloc(SerdWorld* const ignored_world, const unsigned n_quads)
{
  (void)ignored_world;
  (void)n_quads;

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate and insert to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  assert(!check_insert_alloc(&allocator.base));

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0U);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(check_insert_alloc(&allocator.base) == SERD_BAD_ALLOC);
  }

  return 0;
}

static int
test_insert_statements(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  static const size_t n_generate = 20U;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_generate, 0U);

  const size_t n_start = serd_model_size(model);

  {
    // Inserting from the same model does nothing (fast path)
    SerdCursor* const model_begin = serd_model_begin(NULL, model);
    assert(!serd_model_insert_statements(model, model_begin));
    assert(serd_model_size(model) == n_start);
    serd_cursor_free(NULL, model_begin);
  }

  // Make another model to insert from
  SerdModel* const other = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(!add(other, nodes, uri(nodes, 91), uri(nodes, 92), uri(nodes, 93), 0));
  assert(!add(other, nodes, uri(nodes, 94), uri(nodes, 95), uri(nodes, 96), 0));
  const size_t n_other = serd_model_size(other);

  {
    // Inserting an end iterator from another model does nothing (fast path)
    SerdCursor* const other_end = serd_cursor_copy(NULL, serd_model_end(other));
    assert(!serd_model_insert_statements(model, other_end));
    assert(serd_model_size(model) == n_start);
    serd_cursor_free(NULL, other_end);
  }
  {
    // Inserting from another model does
    SerdCursor* const other_begin = serd_model_begin(NULL, other);
    assert(!serd_model_insert_statements(model, other_begin));
    assert(serd_model_size(model) == n_start + n_other);
    serd_cursor_free(NULL, other_begin);
  }

  serd_model_free(other);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_erase_with_iterator(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  serd_world_set_log_func(world, expected_error, NULL);
  assert(!add(model, nodes, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0));
  assert(!add(model, nodes, uri(nodes, 4), uri(nodes, 5), uri(nodes, 6), 0));

  // Erase a statement with an active iterator
  SerdCursor* iter1 = serd_model_begin(NULL, model);
  SerdCursor* iter2 = serd_model_begin(NULL, model);
  assert(!serd_model_erase(model, iter1));

  // Check that erased iterator points to the next statement
  const SerdStatementView s1 = serd_cursor_get(iter1);
  assert(statement_view_matches(
    s1, nodes, uri(nodes, 4), uri(nodes, 5), uri(nodes, 6), 0));

  // Check that other iterator has been invalidated
  // FIXME
  // assert(!serd_cursor_get(iter2).subject);
  assert(serd_cursor_advance(iter2) == SERD_BAD_CURSOR);

  // Check that erasing the end iterator does nothing
  SerdCursor* const end =
    serd_cursor_copy(serd_world_allocator(world), serd_model_end(model));

  assert(serd_model_erase(model, end) == SERD_FAILURE);

  serd_cursor_free(NULL, end);
  serd_cursor_free(NULL, iter2);
  serd_cursor_free(NULL, iter1);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_erase(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  // Add (s p "hello")
  const SerdNodeID s = uri(nodes, 1);
  const SerdNodeID p = uri(nodes, 2);
  const SerdNodeID hello =
    serd_nodes_id(nodes, serd_a_string(zix_string("hello")));

  assert(!add(model, nodes, s, p, hello, 0U));
  assert(serd_model_ask(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, s)),
                        serd_a_token_view(serd_nodes_get_token(nodes, p)),
                        serd_a_token_view(serd_nodes_get_token(nodes, hello)),
                        serd_a_null()));

  // Add (s p "hi")
  const SerdNodeID hi = serd_nodes_id(nodes, serd_a_string(zix_string("hi")));
  assert(!add(model, nodes, s, p, hi, 0U));
  assert(serd_model_ask(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, s)),
                        serd_a_token_view(serd_nodes_get_token(nodes, p)),
                        serd_a_token_view(serd_nodes_get_token(nodes, hi)),
                        serd_a_node_id(0U)));

  // Erase (s p "hi")
  SerdCursor* iter =
    serd_model_find(NULL,
                    model,
                    serd_a_token_view(serd_nodes_get_token(nodes, s)),
                    serd_a_token_view(serd_nodes_get_token(nodes, p)),
                    serd_a_token_view(serd_nodes_get_token(nodes, hi)),
                    serd_a_null());
  assert(iter);
  assert(!serd_model_erase(model, iter));
  assert(serd_model_size(model) == 1);
  serd_cursor_free(NULL, iter);

  // Check that erased statement can't be found
  SerdCursor* empty =
    serd_model_find(NULL,
                    model,
                    serd_a_token_view(serd_nodes_get_token(nodes, s)),
                    serd_a_token_view(serd_nodes_get_token(nodes, p)),
                    serd_a_token_view(serd_nodes_get_token(nodes, hi)),
                    serd_a_null());
  assert(serd_cursor_is_end(empty));
  serd_cursor_free(NULL, empty);

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_from(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  static const ZixStringView doc = ZIX_STATIC_STRING("file.ttl");

  ZixAllocator* const allocator = serd_world_allocator(world);
  SerdNodes* const    nodes     = serd_nodes_new(allocator);

  const SerdNodeID s = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:s")));
  const SerdNodeID p = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:p")));
  const SerdNodeID o = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:o")));
  const SerdNodeID q = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:q")));

  const SerdCaretView origin = {doc, 16, 18};

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_CARETS);

  assert(!add_from(model, nodes, s, p, o, 0U, origin));
  assert(!add_from(model, nodes, s, p, q, 0U, serd_no_caret()));

  SerdCursor* const       cursor     = serd_model_begin(NULL, model);
  const SerdStatementView statement1 = serd_cursor_get(cursor);
  const SerdCaretView     caret1     = serd_cursor_get_caret(cursor);

  // First statement with a caret
  assert(statement_view_matches(statement1, nodes, s, p, o, 0U));
  assert(zix_string_view_equals(caret1.document, doc));
  assert(caret1.line == 16);
  assert(caret1.column == 18);

  // Second statement with no caret
  assert(!serd_cursor_advance(cursor));
  const SerdStatementView statement2 = serd_cursor_get(cursor);
  const SerdCaretView     caret2     = serd_cursor_get_caret(cursor);
  assert(statement_view_matches(statement2, nodes, s, p, q, 0U));
  assert(zix_string_view_equals(caret2.document, zix_empty_string()));
  assert(caret2.line == 0U);
  assert(caret2.column == 0U);

  assert(!serd_model_erase(model, cursor));

  serd_cursor_free(NULL, cursor);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_bad_statement(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(serd_world_allocator(world));
  const SerdNodeID lit =
    serd_nodes_id(nodes, serd_a_string(zix_string("string")));
  const SerdNodeID urn =
    serd_nodes_id(nodes, serd_a_uri(zix_string("urn:uri")));
  const SerdNodeID blank = serd_nodes_id(nodes, serd_a_blank(zix_string("b1")));

  SerdModel* const model = serd_model_new(
    world, SERD_ORDER_SPO, SERD_MODEL_CARETS | SERD_MODEL_GRAPHS);

  // Successfully add a statement with the literal to ensure its interned
  assert(!add(model, nodes, urn, urn, lit, 0U));

  assert(add(model, nodes, lit, urn, urn, 0U));
  assert(add(model, nodes, urn, blank, urn, 0U));
  assert(add(model, nodes, urn, urn, urn, lit));

  // Get the ID of the literal node in the model (not nodes above)
  const SerdNodeID lit_id =
    serd_nodes_existing_id(serd_model_nodes(model),
                           serd_a_token_view(serd_nodes_get_token(nodes, lit)));
  assert(lit_id);

  assert(serd_model_add(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_node_id(0U),
                        serd_a_null()) == SERD_BAD_ARG);
  assert(serd_model_add(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_null(),
                        serd_a_null()) == SERD_BAD_ARG);
  assert(serd_model_add(model,
                        serd_a_node_id(lit_id),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_null()) == SERD_BAD_ARG);
  assert(serd_model_add(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_node_id(lit_id),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_null()) == SERD_BAD_ARG);
  assert(serd_model_add(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_node_id(lit_id)) == SERD_BAD_ARG);
  assert(serd_model_add(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_token_view(serd_nodes_get_token(nodes, urn)),
                        serd_a_node_id(0U)));
  assert(serd_model_add(
           model,
           serd_a_token_view(serd_nodes_get_token(nodes, urn)),
           serd_a_token_view(serd_nodes_get_token(nodes, urn)),
           serd_a_null(),
           serd_a_object(SERD_LITERAL, zix_string(""), 0U, serd_no_token())) ==
         SERD_BAD_ARG);

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_erase_all(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(!serd_model_add_index(model, SERD_ORDER_OSP));
  generate(nodes, model, n_quads, 0U);

  SerdCursor* iter = serd_model_begin(NULL, model);
  while (!serd_cursor_equals(iter, serd_model_end(model))) {
    assert(!serd_model_erase(model, iter));
  }

  assert(serd_model_empty(model));

  serd_cursor_free(NULL, iter);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_clear(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_quads, 0U);

  serd_model_clear(model);
  assert(serd_model_empty(model));

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_copy(SerdWorld* const world, const unsigned n_quads)
{
  // FIXME: test caret copying

  assert(!serd_model_copy(NULL, NULL));

  SerdNodes* const nodes = serd_nodes_new(NULL);
  const SerdNodeID graph = uri(nodes, 42);

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

  generate(nodes, model, n_quads, graph);

  SerdModel* copy = serd_model_copy(serd_world_allocator(world), model);
  assert(copy);
  assert(serd_model_equals(model, copy));

  serd_model_free(model);
  serd_model_free(copy);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_copy_failed_alloc(SerdWorld* const ignored_world, const unsigned n_quads)
{
  (void)ignored_world;
  (void)n_quads;

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdWorld* const     world     = serd_world_new(&allocator.base);
  SerdNodes* const     nodes     = serd_nodes_new(NULL);
  SerdModel* const     model     = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_quads, 0U);

  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  SerdModel* copy = serd_model_copy(serd_world_allocator(world), model);
  assert(copy);
  serd_model_free(copy);

  // Test that each allocation failing is handled gracefully
  const size_t n_copy_allocs = serd_failing_allocator_reset(&allocator, 0U);
  for (size_t i = 0; i < n_copy_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_model_copy(serd_world_allocator(world), model));
  }

  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_world_free(world);
  return 0;
}

static int
test_equals(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_quads, 0U);
  assert(!add(
    model, nodes, uri(nodes, 0), uri(nodes, 1), uri(nodes, 2), uri(nodes, 3)));

  assert(serd_model_equals(NULL, NULL));
  assert(!serd_model_equals(NULL, model));
  assert(!serd_model_equals(model, NULL));

  SerdModel* empty = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(!serd_model_equals(model, empty));

  SerdModel* different = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(nodes, different, n_quads, 0U);
  assert(!add(different,
              nodes,
              uri(nodes, 1),
              uri(nodes, 1),
              uri(nodes, 2),
              uri(nodes, 3)));

  assert(serd_model_size(model) == serd_model_size(different));
  assert(!serd_model_equals(model, different));

  serd_model_free(model);
  serd_model_free(empty);
  serd_model_free(different);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_find_past_end(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  const SerdNodeID s     = uri(nodes, 1);
  const SerdNodeID p     = uri(nodes, 2);
  const SerdNodeID o     = uri(nodes, 3);
  assert(!add(model, nodes, s, p, o, 0U));
  assert(serd_model_ask(model,
                        serd_a_token_view(serd_nodes_get_token(nodes, s)),
                        serd_a_token_view(serd_nodes_get_token(nodes, p)),
                        serd_a_object_view(serd_nodes_get_object(nodes, o)),
                        serd_a_null()));

  const SerdNodeID  huge = uri(nodes, 999);
  SerdCursor* const range =
    serd_model_find(NULL,
                    model,
                    serd_a_token_view(serd_nodes_get_token(nodes, huge)),
                    serd_a_token_view(serd_nodes_get_token(nodes, huge)),
                    serd_a_object_view(serd_nodes_get_object(nodes, huge)),
                    serd_a_null());
  assert(serd_cursor_is_end(range));

  serd_cursor_free(NULL, range);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_find_unknown_node(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID s = uri(nodes, 1);
  const SerdNodeID p = uri(nodes, 2);
  const SerdNodeID o = uri(nodes, 3);

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

  // Add one statement
  assert(!add(model, nodes, s, p, o, 0U));
  assert(ask(model, nodes, s, p, o, 0U));

  /* Test searching for statements that contain a non-existent node.  This is
     semantically equivalent to any other non-matching pattern, but can be
     implemented with a fast path that avoids searching a statement index
     entirely. */

  const SerdNodeID q = uri(nodes, 42);
  assert(!ask(model, nodes, s, p, o, q));
  assert(!ask(model, nodes, s, p, q, 0U));
  assert(!ask(model, nodes, s, q, o, 0U));
  assert(!ask(model, nodes, q, p, o, 0U));

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_find_graph(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID s  = uri(nodes, 1);
  const SerdNodeID p  = uri(nodes, 2);
  const SerdNodeID o1 = uri(nodes, 3);
  const SerdNodeID o2 = uri(nodes, 4);
  const SerdNodeID g  = uri(nodes, 5);

  for (unsigned indexed = 0U; indexed < 2U; ++indexed) {
    SerdModel* const model =
      serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

    if (indexed) {
      assert(!serd_model_add_index(model, SERD_ORDER_GSPO));
    }

    // Add one statement in a named graph and one in the default graph
    assert(!add(model, nodes, s, p, o1, 0U));
    assert(!add(model, nodes, s, p, o2, g));

    // Both statements can be found in the default graph
    assert(ask(model, nodes, s, p, o1, 0U));
    assert(ask(model, nodes, s, p, o2, 0U));

    // Only the one statement can be found in the named graph
    assert(!ask(model, nodes, s, p, o1, g));
    assert(ask(model, nodes, s, p, o2, g));

    // Neither statement can be found in an absent or invalid graph
    assert(!ask(model, nodes, s, p, o1, s));
    assert(!ask(model, nodes, s, p, o2, s));
    assert(!serd_model_ask(
      model,
      serd_a_token_view(serd_nodes_get_token(nodes, s)),
      serd_a_token_view(serd_nodes_get_token(nodes, p)),
      serd_a_object_view(serd_nodes_get_object(nodes, o1)),
      serd_a_object(SERD_LITERAL, zix_string("g"), 0U, serd_no_token())));

    serd_model_free(model);
  }

  serd_nodes_free(nodes);
  return 0;
}

static int
test_find_bad_arg(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_GSPO, SERD_MODEL_GRAPHS);
  generate(nodes, model, n_quads, 0U);

  const SerdNodeArgs u = serd_a_uri(zix_string(NS_EG "u"));

  assert(!serd_model_find(
    NULL, model, serd_a_token(SERD_LITERAL, zix_string("s")), u, u, u));
  assert(!serd_model_find(
    NULL, model, u, serd_a_token(SERD_LITERAL, zix_string("s")), u, u));
  assert(!serd_model_find(
    NULL, model, u, u, u, serd_a_token(SERD_LITERAL, zix_string("s"))));

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_range(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_quads, 0U);

  SerdCursor* range1 = serd_model_begin(NULL, model);
  SerdCursor* range2 = serd_model_begin(NULL, model);

  assert(!serd_cursor_is_end(range1));
  assert(serd_cursor_is_end(NULL));

  assert(serd_cursor_equals(NULL, NULL));
  assert(!serd_cursor_equals(range1, NULL));
  assert(!serd_cursor_equals(NULL, range1));
  assert(serd_cursor_equals(range1, range2));

  assert(!serd_cursor_advance(range2));
  assert(!serd_cursor_equals(range1, range2));

  serd_cursor_free(NULL, range2);
  serd_cursor_free(NULL, range1);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_triple_index_read(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  serd_world_set_log_func(world, ignore_only_index_error, NULL);

  for (unsigned i = 0; i < 6; ++i) {
    SerdModel* const model = serd_model_new(world, (SerdStatementOrder)i, 0U);
    generate(nodes, model, n_quads, 0);
    assert(!test_read(model, nodes, 0, n_quads));
    serd_model_free(model);
  }

  serd_nodes_free(nodes);
  return 0;
}

static int
test_quad_index_read(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  serd_world_set_log_func(world, ignore_only_index_error, NULL);

  for (unsigned i = 0; i < 6; ++i) {
    SerdModel* const model =
      serd_model_new(world, (SerdStatementOrder)i, SERD_MODEL_GRAPHS);

    const SerdNodeID graph = uri(nodes, 42);
    generate(nodes, model, n_quads, graph);
    assert(!test_read(model, nodes, graph, n_quads));
    serd_model_free(model);
  }

  serd_nodes_free(nodes);
  return 0;
}

static int
test_remove_graph(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_GSPO, SERD_MODEL_GRAPHS);

  // Generate a couple of graphs
  const SerdNodeID graph42 = uri(nodes, 42);
  const SerdNodeID graph43 = uri(nodes, 43);
  generate(nodes, model, 1, graph42);
  generate(nodes, model, 1, graph43);

  // Find the start of graph43
  SerdCursor* range =
    serd_model_find(NULL,
                    model,
                    serd_a_null(),
                    serd_a_null(),
                    serd_a_null(),
                    serd_a_token_view(serd_nodes_get_token(nodes, graph43)));
  assert(range);

  // Remove the entire range of statements in the graph
  SerdStatus st = serd_model_erase_statements(model, range);
  assert(!st);
  serd_cursor_free(NULL, range);

  // Erase the first tuple (an element in the default graph)
  SerdCursor* iter = serd_model_begin(NULL, model);
  assert(!serd_model_erase(model, iter));
  serd_cursor_free(NULL, iter);

  // Ensure only the other graph is left
  const Quad pat = {0, 0, 0, graph42};
  for (iter = serd_model_begin(NULL, model);
       !serd_cursor_equals(iter, serd_model_end(model));
       serd_cursor_advance(iter)) {
    const SerdStatementView s = serd_cursor_get(iter);
    assert(statement_view_matches(s, nodes, pat[0], pat[1], pat[2], pat[3]));
  }
  serd_cursor_free(NULL, iter);

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_default_graph(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID s  = uri(nodes, 1);
  const SerdNodeID p  = uri(nodes, 2);
  const SerdNodeID o  = uri(nodes, 3);
  const SerdNodeID g1 = uri(nodes, 101);
  const SerdNodeID g2 = uri(nodes, 102);

  {
    // Make a model that does not store graphs
    SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

    // Insert a statement into a graph (which will be dropped)
    assert(!add(model, nodes, s, p, o, g1));

    // Attempt to insert the same statement into another graph
    assert(add(model, nodes, s, p, o, g2) == SERD_FAILURE);

    // Ensure that we only see the statement once
    assert(serd_model_count(model,
                            serd_a_token_view(serd_nodes_get_token(nodes, s)),
                            serd_a_token_view(serd_nodes_get_token(nodes, p)),
                            serd_a_token_view(serd_nodes_get_token(nodes, o)),
                            serd_a_null()) == 1);

    serd_model_free(model);
  }

  {
    // Make a model that stores graphs
    SerdModel* const model =
      serd_model_new(world, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

    // Insert the same statement into two graphs
    assert(!add(model, nodes, s, p, o, g1));
    assert(!add(model, nodes, s, p, o, g2));

    // Ensure we see the statement twice
    assert(serd_model_count(model,
                            serd_a_token_view(serd_nodes_get_token(nodes, s)),
                            serd_a_token_view(serd_nodes_get_token(nodes, p)),
                            serd_a_token_view(serd_nodes_get_token(nodes, o)),
                            serd_a_null()) == 2);

    serd_model_free(model);
  }

  serd_nodes_free(nodes);
  return 0;
}

int
main(void)
{
  static const unsigned n_quads = 300;

  typedef int (*TestFunc)(SerdWorld*, unsigned);

  const TestFunc tests[] = {test_free_null,
                            test_get_world,
                            test_get_default_order,
                            test_get_flags,
                            test_all_begin,
                            test_begin_ordered,
                            test_add_with_iterator,
                            test_add_remove_nodes,
                            test_add_index,
                            test_remove_index,
                            test_inserter,
                            test_insert_failed_alloc,
                            test_insert_statements,
                            test_erase_with_iterator,
                            test_add_erase,
                            test_add_from,
                            test_add_bad_statement,
                            test_erase_all,
                            test_clear,
                            test_copy,
                            test_copy_failed_alloc,
                            test_equals,
                            test_find_past_end,
                            test_find_unknown_node,
                            test_find_graph,
                            test_find_bad_arg,
                            test_range,
                            test_triple_index_read,
                            test_quad_index_read,
                            test_remove_graph,
                            test_default_graph,
                            NULL};

  SerdWorld* world = serd_world_new(NULL);
  int        ret   = 0;

  for (const TestFunc* t = tests; *t; ++t) {
    serd_world_set_log_func(world, NULL, NULL);
    ret += (*t)(world, n_quads);
  }

  serd_world_free(world);
  return ret;
}
