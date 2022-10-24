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
#include <serd/model_caret.h>
#include <serd/node_args.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/strings.h>
#include <serd/struct_literal.h>
#include <serd/token_view.h>
#include <serd/tuple.h>
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
add_from(SerdModel* const     model,
         const SerdNodeID     s,
         const SerdNodeID     p,
         const SerdNodeID     o,
         const SerdNodeID     g,
         const SerdModelCaret caret)
{
  return serd_model_insert_tuple(
    model, SERD_STRUCT_LITERAL(SerdTuple, {s, p, o, g}), caret);
}

static SerdStatus
insert(SerdModel* const model,
       const SerdNodeID s,
       const SerdNodeID p,
       const SerdNodeID o,
       const SerdNodeID g)
{
  return serd_model_insert(model, s, p, o, g);
}

static SerdNodeID
find_node(const SerdModel* const model,
          const SerdNodeID       s,
          const SerdNodeID       p,
          const SerdNodeID       o,
          const SerdNodeID       g)
{
  return serd_model_find_node(model, s, p, o, g);
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
      st = insert(model, ids[0], ids[1], ids[2 + j], graph);
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

  assert(!insert(model, uri(nodes, 98), uri(nodes, 4), hello, graph));
  assert(!insert(model, uri(nodes, 98), uri(nodes, 4), hello_t5, graph));

  // (96 4 "hello"^^<4>) and (96 4 "hello"^^<5>)
  assert(!insert(model, uri(nodes, 96), uri(nodes, 4), hello_t4, graph));
  assert(!insert(model, uri(nodes, 96), uri(nodes, 4), hello_t5, graph));

  // (94 5 "hello") and (94 5 "hello"@en-gb)
  assert(!insert(model, uri(nodes, 94), uri(nodes, 5), hello, graph));
  assert(!insert(model, uri(nodes, 94), uri(nodes, 5), hello_gb, graph));

  // (92 6 "hello"@en-us) and (92 6 "hello"@en-gb)
  assert(!insert(model, uri(nodes, 92), uri(nodes, 6), hello_us, graph));
  assert(!insert(model, uri(nodes, 92), uri(nodes, 6), hello_gb, graph));

  // (14 6 "bonjour"@fr) and (14 6 "salut"@fr)

  const SerdNodeID bonjour = serd_nodes_id(
    nodes, serd_a_plain_literal(zix_string("bonjour"), zix_string("fr")));

  const SerdNodeID salut = serd_nodes_id(
    nodes, serd_a_plain_literal(zix_string("salut"), zix_string("fr")));

  assert(!insert(model, uri(nodes, 14), uri(nodes, 6), bonjour, graph));
  assert(!insert(model, uri(nodes, 14), uri(nodes, 6), salut, graph));

  // Attempt to add duplicates
  assert(insert(model, uri(nodes, 14), uri(nodes, 6), salut, graph));

  // Add a blank node subject
  const SerdNodeID ablank =
    serd_nodes_id(nodes, serd_a_blank(zix_string("ablank")));

  assert(!insert(model, ablank, uri(nodes, 6), salut, graph));

  // Add statement with URI object
  assert(!insert(model, ablank, uri(nodes, 6), uri(nodes, 7), graph));

  return EXIT_SUCCESS;
}

static bool
required_id_matches(const SerdNodeID result_id, const SerdNodeID query_id)
{
  assert(result_id);
  return !query_id || query_id == result_id;
}

static bool
optional_id_matches(const SerdNodeID result_id, const SerdNodeID query_id)
{
  return !query_id || query_id == result_id;
}

static bool
cursor_statement_matches(const SerdCursor* const cursor,
                         const SerdNodeID        s,
                         const SerdNodeID        p,
                         const SerdNodeID        o,
                         const SerdNodeID        g)
{
  const SerdTuple tuple = serd_cursor_tuple(cursor);
  return (!s || tuple.nodes[0] == s) && (!p || tuple.nodes[1] == p) &&
         (!o || tuple.nodes[2] == o) && (!g || tuple.nodes[3] == g);
}

static int
test_read(const SerdModel* const model,
          SerdNodes* const       nodes,
          const SerdNodeID       g,
          const unsigned         n_quads)
{
  SerdCursor* const cursor = serd_model_begin(NULL, model);
  SerdTuple         prev   = {{0U, 0U, 0U, 0U}};

  for (; !serd_cursor_equals(cursor, serd_model_end(model));
       serd_cursor_advance(cursor)) {
    const SerdTuple tuple = serd_cursor_tuple(cursor);
    assert(!(
      (tuple.nodes[0] == prev.nodes[0]) && (tuple.nodes[1] == prev.nodes[1]) &&
      (tuple.nodes[2] == prev.nodes[2]) && (tuple.nodes[3] == prev.nodes[3])));

    assert(serd_field_supports(SERD_SUBJECT,
                               serd_nodes_type(nodes, tuple.nodes[0])));
    assert(serd_field_supports(SERD_PREDICATE,
                               serd_nodes_type(nodes, tuple.nodes[1])));
    assert(
      serd_field_supports(SERD_OBJECT, serd_nodes_type(nodes, tuple.nodes[2])));
    assert(
      !tuple.nodes[3] ||
      serd_field_supports(SERD_GRAPH, serd_nodes_type(nodes, tuple.nodes[3])));
    prev = tuple;
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
  assert(serd_model_ask(model, match[0], match[1], match[2], match[3]));

  const Quad nomatch = {uri(nodes, 1), uri(nodes, 2), uri(nodes, 9), g};
  assert(
    !serd_model_ask(model, nomatch[0], nomatch[1], nomatch[2], nomatch[3]));

  assert(!find_node(model, 0U, 0U, uri(nodes, 3), g));
  assert(!find_node(model, uri(nodes, 1), uri(nodes, 99), 0U, g));

  assert(find_node(model, uri(nodes, 1), uri(nodes, 2), 0U, g) ==
         uri(nodes, 3));
  assert(find_node(model, uri(nodes, 1), 0U, uri(nodes, 3), g) ==
         uri(nodes, 2));
  assert(find_node(model, 0U, uri(nodes, 2), uri(nodes, 3), g) ==
         uri(nodes, 1));

  if (g) {
    assert(find_node(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0U) ==
           g);
  }

  for (unsigned i = 0; i < NUM_PATTERNS; ++i) {
    QueryTest  test = patterns[i];
    const Quad pat  = {test.query[0], test.query[1], test.query[2], g};

    SerdCursor* const range =
      serd_model_find(NULL, model, pat[0], pat[1], pat[2], pat[3]);

    unsigned num_results = 0U;
    for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
      const SerdTuple tuple = serd_cursor_tuple(range);
      assert(required_id_matches(tuple.nodes[0], pat[0]));
      assert(required_id_matches(tuple.nodes[1], pat[1]));
      assert(required_id_matches(tuple.nodes[2], pat[2]));
      assert(optional_id_matches(tuple.nodes[3], pat[3]));
      ++num_results;
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
    serd_model_find(NULL, model, pat[0], pat[1], pat[2], pat[3]);

  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    const SerdTuple tuple = serd_cursor_tuple(range);
    assert(required_id_matches(tuple.nodes[0], pat[0]));
    assert(required_id_matches(tuple.nodes[1], pat[1]));
    assert(required_id_matches(tuple.nodes[2], pat[2]));
    assert(optional_id_matches(tuple.nodes[3], pat[3]));
    ++num_results;
  }
  serd_cursor_free(NULL, range);

  assert(num_results == 2U);

  // Test nested queries
  SerdNodeID last_subject = 0U;

  range = serd_model_find(NULL, model, 0, 0, 0, 0);
  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    const SerdTuple  tuple   = serd_cursor_tuple(range);
    const SerdNodeID subject = tuple.nodes[0];
    if (subject == last_subject) {
      continue;
    }

    const Quad        subpat = {subject, 0U, 0U, 0U};
    SerdCursor* const subrange =
      serd_model_find(NULL, model, subpat[0], subpat[1], subpat[2], subpat[3]);

    assert(subrange);

    const SerdTuple substatement    = serd_cursor_tuple(subrange);
    uint64_t        num_sub_results = 0;
    assert(substatement.nodes[0] == subject);
    for (; !serd_cursor_is_end(subrange); serd_cursor_advance(subrange)) {
      assert(cursor_statement_matches(
        subrange, subpat[0], subpat[1], subpat[2], subpat[3]));

      ++num_sub_results;
    }
    serd_cursor_free(NULL, subrange);
    assert(num_sub_results == N_OBJECTS_PER);

    const size_t count = serd_model_count(model, subject, 0, 0, 0);
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

  SerdNodes* const nodes = serd_nodes_new(NULL);
  assert(nodes);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(model);
  assert(serd_model_world(model) == world);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_get_default_order(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  assert(nodes);

  SerdModel* const model1 = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(model1);

  SerdModel* const model2 =
    serd_model_new(world, nodes, SERD_ORDER_GPSO, SERD_MODEL_GRAPHS);
  assert(model2);

  assert(serd_model_default_order(model1) == SERD_ORDER_SPO);
  assert(serd_model_default_order(model2) == SERD_ORDER_GPSO);

  serd_model_free(model2);
  serd_model_free(model1);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_get_flags(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  assert(nodes);

  const SerdModelFlags flags = SERD_MODEL_GRAPHS | SERD_MODEL_CARETS;
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, flags);
  assert(model);

  assert(serd_model_flags(model) & SERD_MODEL_GRAPHS);
  assert(serd_model_flags(model) & SERD_MODEL_CARETS);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_all_begin(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(model);

  SerdCursor* const begin = serd_model_begin(NULL, model);
  SerdCursor* const first = serd_model_find(NULL, model, 0, 0, 0, 0);

  assert(serd_cursor_equals(begin, first));

  serd_cursor_free(NULL, first);
  serd_cursor_free(NULL, begin);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_begin_ordered(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);
  assert(model);

  assert(!insert(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0U));

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
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(model);

  serd_world_set_log_func(world, expected_error, NULL);
  assert(!insert(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0U));

  // Add a statement with an active iterator
  SerdCursor* iter = serd_model_begin(NULL, model);
  assert(!insert(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 4), 0U));

  // Check that iterator has been invalidated
  const SerdTuple tuple = serd_cursor_tuple(iter);
  assert(!tuple.nodes[0]);
  assert(!tuple.nodes[1]);
  assert(!tuple.nodes[2]);
  assert(!tuple.nodes[3]);
  assert(serd_cursor_advance(iter) == SERD_BAD_CURSOR);

  serd_cursor_free(NULL, iter);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_index(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
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
  assert(!insert(model, s, p, o1, 0U));
  assert(!insert(model, s, p, o2, 0U));
  assert(serd_model_size(model) == 2);

  // Add a new index
  assert(!serd_model_has_index(model, SERD_ORDER_PSO));
  assert(!serd_model_add_index(model, SERD_ORDER_PSO));
  assert(serd_model_has_index(model, SERD_ORDER_PSO));

  // Count statements via the new index
  size_t            count = 0U;
  SerdCursor* const cur   = serd_model_find(NULL, model, 0, p, 0, 0);
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
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(model);

  const SerdNodeID s  = uri(nodes, 0);
  const SerdNodeID p  = uri(nodes, 1);
  const SerdNodeID o1 = uri(nodes, 2);
  const SerdNodeID o2 = uri(nodes, 3);

  // Try to remove default and non-existent indices
  assert(serd_model_drop_index(model, SERD_ORDER_SPO) == SERD_BAD_CALL);
  assert(serd_model_drop_index(model, SERD_ORDER_PSO) == SERD_FAILURE);

  // Add a couple of statements so that dropping an index isn't trivial
  assert(!insert(model, s, p, o1, 0U));
  assert(!insert(model, s, p, o2, 0U));
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

  SerdNodes* const nodes = serd_nodes_new(NULL);
  assert(nodes);

  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);
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

  SerdCursor* const iter  = serd_model_begin(NULL, model);
  const SerdTuple   first = serd_cursor_tuple(iter);
  assert(first.nodes[0] == serd_nodes_find(nodes, serd_a_token_view(s)));
  assert(first.nodes[1] == serd_nodes_find(nodes, serd_a_token_view(p)));
  assert(first.nodes[2] == serd_nodes_find(nodes, serd_a_object_view(o)));
  assert(first.nodes[3] == serd_nodes_find(nodes, serd_a_token_view(g)));
  serd_cursor_free(NULL, iter);

  serd_handler_free(inserter);
  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_env_free(env);
  return 0;
}

static SerdStatus
check_insert_alloc(ZixAllocator* const allocator)
{
  static const SerdCaretView caret = {ZIX_STATIC_STRING("doc"), 1, 1};
  static const SerdTokenView s     = {SERD_URI, ZIX_STATIC_STRING(NS_EG "s")};
  static const SerdTokenView p     = {SERD_URI, ZIX_STATIC_STRING(NS_EG "p")};
  static const SerdTokenView g     = {SERD_URI, ZIX_STATIC_STRING(NS_EG "g")};

  char           o_buf[32] = {'\0'};
  SerdObjectView o         = {SERD_LITERAL,
                              zix_string(o_buf),
                              SERD_HAS_DATATYPE,
                              {SERD_URI, ZIX_STATIC_STRING(NS_EG "T")}};

  SerdWorld* const world = serd_world_new(allocator);
  SerdNodes* const nodes = world ? serd_nodes_new(allocator) : NULL;
  SerdEnv* const   env =
    nodes ? serd_env_new(allocator, zix_empty_string()) : NULL;
  if (!world || !nodes || !env) {
    serd_env_free(env);
    serd_nodes_free(nodes);
    serd_world_free(world);
    return SERD_BAD_ALLOC;
  }

  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_CARETS);

  SerdStatus st = model ? SERD_SUCCESS : SERD_BAD_ALLOC;

  if (!st) {
    st = serd_model_add_index(model, SERD_ORDER_OSP);
  }

  if (!st) {
    SerdHandler* const inserter = serd_inserter_new(model, env, g);
    if (!inserter) {
      st = SERD_BAD_ALLOC;
    } else {
      for (unsigned i = 0U; i < 4U; ++i) {
        snprintf(o_buf, sizeof(o_buf), "o%u", i);
        o.string = zix_string(o_buf);

        const SerdEvent event = serd_cite_event(
          serd_statement_event(0U, serd_quad_view(s, p, o, g)), caret);

        st = serd_sink_event(serd_handler_sink(inserter), event);
      }
      serd_handler_free(inserter);
    }
  }

  serd_model_free(model);
  serd_env_free(env);
  serd_nodes_free(nodes);
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
    const SerdStatus st = check_insert_alloc(&allocator.base);
    assert(st == SERD_BAD_ALLOC || st == SERD_BAD_DATA);
  }

  return 0;
}

static int
test_insert_range(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  static const size_t n_generate = 20U;

  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_generate, 0U);

  const size_t n_start = serd_model_size(model);

  {
    // Inserting from the same model does nothing (fast path)
    SerdCursor* const model_begin = serd_model_begin(NULL, model);
    assert(!serd_model_insert_range(model, model_begin));
    assert(serd_model_size(model) == n_start);
    serd_cursor_free(NULL, model_begin);
  }

  // Make another model to insert from
  SerdModel* const other = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(!insert(other, uri(nodes, 91), uri(nodes, 92), uri(nodes, 93), 0));
  assert(!insert(other, uri(nodes, 94), uri(nodes, 95), uri(nodes, 96), 0));
  const size_t n_other = serd_model_size(other);

  {
    // Inserting an end iterator from another model does nothing (fast path)
    SerdCursor* const other_end = serd_cursor_copy(NULL, serd_model_end(other));
    assert(!serd_model_insert_range(model, other_end));
    assert(serd_model_size(model) == n_start);
    serd_cursor_free(NULL, other_end);
  }
  {
    // Inserting from another model does
    SerdCursor* const other_begin = serd_model_begin(NULL, other);
    assert(!serd_model_insert_range(model, other_begin));
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
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(model);

  serd_world_set_log_func(world, expected_error, NULL);
  assert(!insert(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0));
  assert(!insert(model, uri(nodes, 4), uri(nodes, 5), uri(nodes, 6), 0));

  // Erase a statement with active iterators
  SerdCursor* iter1 = serd_model_begin(NULL, model);
  SerdCursor* iter2 = serd_model_begin(NULL, model);
  SerdCursor* iter3 = serd_model_begin(NULL, model);
  assert(!serd_model_erase(model, iter1));

  // Check that erased iterator points to the next statement
  assert(cursor_statement_matches(
    iter1, uri(nodes, 4), uri(nodes, 5), uri(nodes, 6), 0));

  // Check that other iterators have been invalidated
  assert(!serd_cursor_tuple(iter2).nodes[0]);
  assert(serd_cursor_advance(iter2) == SERD_BAD_CURSOR);
  assert(serd_model_erase_range(model, iter3) == SERD_FAILURE);

  // Check that erasing the end iterator does nothing
  SerdCursor* const end =
    serd_cursor_copy(serd_world_allocator(world), serd_model_end(model));

  assert(serd_model_erase(model, end) == SERD_FAILURE);

  serd_cursor_free(NULL, end);
  serd_cursor_free(NULL, iter3);
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
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(model);

  // Add (s p "hello")
  const SerdNodeID s = uri(nodes, 1);
  const SerdNodeID p = uri(nodes, 2);
  const SerdNodeID hello =
    serd_nodes_id(nodes, serd_a_string(zix_string("hello")));

  assert(!insert(model, s, p, hello, 0));
  assert(serd_model_ask(model, s, p, hello, 0));

  // Add (s p "hi")
  const SerdNodeID hi = serd_nodes_id(nodes, serd_a_string(zix_string("hi")));
  assert(!insert(model, s, p, hi, 0));
  assert(serd_model_ask(model, s, p, hi, 0));

  // Erase (s p "hi")
  SerdCursor* iter = serd_model_find(NULL, model, s, p, hi, 0);
  assert(iter);
  assert(!serd_model_erase(model, iter));
  assert(serd_model_size(model) == 1);
  serd_cursor_free(NULL, iter);

  // Check that erased statement can't be found
  SerdCursor* empty = serd_model_find(NULL, model, s, p, hi, 0);
  assert(serd_cursor_is_end(empty));
  serd_cursor_free(NULL, empty);

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_insert_from(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  static const ZixStringView doc = ZIX_STATIC_STRING("file.ttl");

  ZixAllocator* const allocator = serd_world_allocator(world);
  SerdNodes* const    nodes     = serd_nodes_new(allocator);
  assert(nodes);

  const SerdNodeID s = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:s")));
  const SerdNodeID p = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:p")));
  const SerdNodeID o = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:o")));
  const SerdNodeID q = serd_nodes_id(nodes, serd_a_uri(zix_string("urn:q")));

  const SerdNodeID     d      = serd_nodes_id(nodes, serd_a_string(doc));
  const SerdModelCaret origin = {d, 16, 18};

  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_CARETS);

  assert(!add_from(model, s, p, o, 0U, origin));

  const SerdModelCaret no_caret = {0U, 0U, 0U};
  assert(!add_from(model, s, p, q, 0U, no_caret));

  SerdStrings* const strings =
    serd_strings_new(NULL, serd_model_mutable_nodes(model));

  SerdCursor* const    cursor = serd_model_begin(NULL, model);
  const SerdModelCaret caret1 = serd_cursor_caret(cursor);

  // First statement with a caret
  assert(cursor_statement_matches(cursor, s, p, o, 0U));
  assert(
    zix_string_view_equals(serd_strings_caret(strings, caret1).document, doc));
  assert(caret1.line == 16);
  assert(caret1.column == 18);

  // Second statement with no caret
  assert(!serd_cursor_advance(cursor));
  const SerdModelCaret caret2 = serd_cursor_caret(cursor);
  assert(cursor_statement_matches(cursor, s, p, q, 0U));
  assert(zix_string_view_equals(serd_strings_caret(strings, caret2).document,
                                zix_empty_string()));
  assert(caret2.line == 0U);
  assert(caret2.column == 0U);

  assert(!serd_model_erase(model, cursor));

  serd_cursor_free(NULL, cursor);
  serd_strings_free(strings);
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
    world, nodes, SERD_ORDER_SPO, SERD_MODEL_CARETS | SERD_MODEL_GRAPHS);

  // Successfully add a statement with these nodes
  assert(!insert(model, urn, urn, lit, 0U));

  // Invalid node type for field
  assert(insert(model, lit, urn, urn, 0U) == SERD_BAD_ARG);
  assert(insert(model, urn, blank, urn, 0U) == SERD_BAD_ARG);
  assert(insert(model, urn, urn, urn, lit) == SERD_BAD_ARG);

  // Missing argument
  assert(insert(model, 0U, urn, urn, 0U) == SERD_BAD_ARG);
  assert(insert(model, urn, 0U, urn, 0U) == SERD_BAD_ARG);
  assert(insert(model, urn, urn, 0U, 0U) == SERD_BAD_ARG);

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_erase_all(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
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
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
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

  SerdNodes* const nodes = serd_nodes_new(NULL);
  assert(nodes);
  assert(!serd_model_copy(NULL, nodes, NULL));

  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

  const SerdNodeID graph = uri(nodes, 42);
  generate(nodes, model, n_quads, graph);

  { // Copy with same nodes
    SerdModel* copy1 =
      serd_model_copy(serd_world_allocator(world), nodes, model);
    assert(copy1);
    assert(serd_model_equals(model, copy1));
    serd_model_free(copy1);
  }

  { // Copy with different nodes
    SerdNodes* nodes2 = serd_nodes_new(NULL);
    SerdModel* copy2 =
      serd_model_copy(serd_world_allocator(world), nodes2, model);
    assert(copy2);
    assert(serd_model_equals(model, copy2));
    serd_model_free(copy2);
    serd_nodes_free(nodes2);
  }

  serd_model_free(model);
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
  SerdModel* const     model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_quads, 0U);

  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  SerdModel* copy = serd_model_copy(serd_world_allocator(world), nodes, model);
  assert(copy);
  serd_model_free(copy);

  // Test that each allocation failing is handled gracefully
  const size_t n_copy_allocs = serd_failing_allocator_reset(&allocator, 0U);
  for (size_t i = 0; i < n_copy_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_model_copy(serd_world_allocator(world), nodes, model));
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
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  generate(nodes, model, n_quads, 0U);
  assert(
    !insert(model, uri(nodes, 0), uri(nodes, 1), uri(nodes, 2), uri(nodes, 3)));

  assert(serd_model_equals(NULL, NULL));
  assert(!serd_model_equals(NULL, model));
  assert(!serd_model_equals(model, NULL));

  SerdModel* empty = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  assert(!serd_model_equals(model, empty));
  assert(!serd_model_equals(empty, model));

  SerdModel* different = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  generate(nodes, different, n_quads, 0U);
  assert(!insert(
    different, uri(nodes, 1), uri(nodes, 1), uri(nodes, 2), uri(nodes, 3)));

  assert(serd_model_size(model) == serd_model_size(different));
  assert(!serd_model_equals(model, different));
  assert(!serd_model_equals(different, model));

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
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  const SerdNodeID s     = uri(nodes, 1);
  const SerdNodeID p     = uri(nodes, 2);
  const SerdNodeID o     = uri(nodes, 3);
  assert(!insert(model, s, p, o, 0U));
  assert(serd_model_ask(model, s, p, o, 0));

  const SerdNodeID  huge  = uri(nodes, 999);
  SerdCursor* const range = serd_model_find(NULL, model, huge, huge, huge, 0);
  assert(serd_cursor_is_end(range));

  serd_cursor_free(NULL, range);
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
      serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

    if (indexed) {
      assert(!serd_model_add_index(model, SERD_ORDER_GSPO));
    }

    // Add one statement in a named graph and one in the default graph
    assert(!insert(model, s, p, o1, 0U));
    assert(!insert(model, s, p, o2, g));

    // Both statements can be found in the default graph
    assert(serd_model_ask(model, s, p, o1, 0U));
    assert(serd_model_ask(model, s, p, o2, 0U));

    // Only the one statement can be found in the named graph
    assert(!serd_model_ask(model, s, p, o1, g));
    assert(serd_model_ask(model, s, p, o2, g));

    // Neither statement can be found in an absent graph
    assert(!serd_model_ask(model, s, p, o1, s));
    assert(!serd_model_ask(model, s, p, o2, s));

    serd_model_free(model);
  }

  serd_nodes_free(nodes);
  return 0;
}

static int
test_find_graph_wildcard(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(NULL);

  /* Covers the case where graph pattern wildcard matching isn't properly
     implemented, which is pretty subtle and easy to miss.  Break
     node_id_graph_wildcard_compare() to observe failure. */

  for (unsigned count = 500U; count < 750U; ++count) {
    SerdModel* const model =
      serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

    assert(!serd_model_add_index(model, SERD_ORDER_GSPO));

    for (unsigned i = 0U; i < count; ++i) {
      assert(!insert(model,
                     uri(nodes, i + count),
                     uri(nodes, i + (2U * count)),
                     uri(nodes, i + (3U * count)),
                     uri(nodes, i + (4U * count))));
    }

    for (unsigned i = 0U; i < count; ++i) {
      assert(serd_model_ask(model,
                            uri(nodes, i + count),
                            uri(nodes, i + (2U * count)),
                            uri(nodes, i + (3U * count)),
                            0));
      assert(serd_model_ask(model,
                            uri(nodes, i + count),
                            uri(nodes, i + (2U * count)),
                            uri(nodes, i + (3U * count)),
                            uri(nodes, i + (4U * count))));
    }

    serd_model_free(model);
  }

  serd_nodes_free(nodes);
  return 0;
}

static int
test_range(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
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
    SerdModel* const model =
      serd_model_new(world, nodes, (SerdStatementOrder)i, 0U);
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
      serd_model_new(world, nodes, (SerdStatementOrder)i, SERD_MODEL_GRAPHS);

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
    serd_model_new(world, nodes, SERD_ORDER_GSPO, SERD_MODEL_GRAPHS);

  // Generate a couple of graphs
  const SerdNodeID graph42 = uri(nodes, 42);
  const SerdNodeID graph43 = uri(nodes, 43);
  generate(nodes, model, 1, graph42);
  generate(nodes, model, 1, graph43);

  // Find the start of graph43
  SerdCursor* range = serd_model_find(NULL, model, 0, 0, 0, graph43);
  assert(range);

  // Remove the entire range of statements in the graph
  SerdStatus st = serd_model_erase_range(model, range);
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
    assert(cursor_statement_matches(iter, pat[0], pat[1], pat[2], pat[3]));
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
    SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);

    // Insert a statement into a graph (which will be dropped)
    assert(!insert(model, s, p, o, g1));

    // Attempt to insert the same statement into another graph
    assert(insert(model, s, p, o, g2) == SERD_FAILURE);

    // Ensure that we only see the statement once
    assert(serd_model_count(model, s, p, o, 0) == 1);

    serd_model_free(model);
  }

  {
    // Make a model that stores graphs
    SerdModel* const model =
      serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

    // Insert the same statement into two graphs
    assert(!insert(model, s, p, o, g1));
    assert(!insert(model, s, p, o, g2));

    // Ensure we see the statement twice
    assert(serd_model_count(model, s, p, o, 0) == 2);

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
                            test_add_index,
                            test_remove_index,
                            test_inserter,
                            test_insert_failed_alloc,
                            test_insert_range,
                            test_erase_with_iterator,
                            test_add_erase,
                            test_insert_from,
                            test_add_bad_statement,
                            test_erase_all,
                            test_clear,
                            test_copy,
                            test_copy_failed_alloc,
                            test_equals,
                            test_find_past_end,
                            test_find_graph,
                            test_find_graph_wildcard,
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
