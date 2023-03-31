// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/buffer.h"
#include "serd/caret_view.h"
#include "serd/cursor.h"
#include "serd/describe.h"
#include "serd/env.h"
#include "serd/inserter.h"
#include "serd/log.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/output_stream.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WILDCARD_NODE NULL

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define RDF_FIRST NS_RDF "first"
#define RDF_REST NS_RDF "rest"
#define RDF_NIL NS_RDF "nil"

#define N_OBJECTS_PER 2U

typedef const SerdNode* Quad[4];

typedef struct {
  Quad     query;
  unsigned expected_num_results;
} QueryTest;

static const SerdNode*
uri(SerdNodes* const nodes, const unsigned num)
{
  char str[] = "http://example.org/0000000000";

  snprintf(str + 19, 11, "%010u", num);

  return serd_nodes_get(nodes, serd_a_uri_string(str));
}

static int
generate(SerdWorld* const      world,
         SerdModel* const      model,
         const size_t          n_quads,
         const SerdNode* const graph)
{
  SerdNodes* const nodes = serd_world_nodes(world);
  SerdStatus       st    = SERD_SUCCESS;

  for (unsigned i = 0; i < n_quads; ++i) {
    unsigned num = (i * N_OBJECTS_PER) + 1U;

    const SerdNode* ids[2 + N_OBJECTS_PER];
    for (unsigned j = 0; j < 2 + N_OBJECTS_PER; ++j) {
      ids[j] = uri(nodes, num++);
    }

    for (unsigned j = 0; j < N_OBJECTS_PER; ++j) {
      st = serd_model_add(model, ids[0], ids[1], ids[2 + j], graph);
      assert(!st);
    }
  }

  // Add some literals

  const SerdNode* const en_gb = serd_nodes_get(nodes, serd_a_string("en-gb"));
  const SerdNode* const en_us = serd_nodes_get(nodes, serd_a_string("en-us"));
  const SerdNode* const fr    = serd_nodes_get(nodes, serd_a_string("fr"));

  // (98 4 "hello") and (98 4 "hello"^^<5>)
  const SerdNode* const hello = serd_nodes_get(nodes, serd_a_string("hello"));

  const SerdNode* const hello_gb =
    serd_nodes_get(nodes, serd_a_plain_literal(zix_string("hello"), en_gb));

  const SerdNode* const hello_us =
    serd_nodes_get(nodes, serd_a_plain_literal(zix_string("hello"), en_us));

  const SerdNode* const hello_t4 = serd_nodes_get(
    nodes, serd_a_typed_literal(zix_string("hello"), uri(nodes, 4)));

  const SerdNode* const hello_t5 = serd_nodes_get(
    nodes, serd_a_typed_literal(zix_string("hello"), uri(nodes, 5)));

  assert(!serd_model_add(model, uri(nodes, 98), uri(nodes, 4), hello, graph));
  assert(
    !serd_model_add(model, uri(nodes, 98), uri(nodes, 4), hello_t5, graph));

  // (96 4 "hello"^^<4>) and (96 4 "hello"^^<5>)
  assert(
    !serd_model_add(model, uri(nodes, 96), uri(nodes, 4), hello_t4, graph));
  assert(
    !serd_model_add(model, uri(nodes, 96), uri(nodes, 4), hello_t5, graph));

  // (94 5 "hello") and (94 5 "hello"@en-gb)
  assert(!serd_model_add(model, uri(nodes, 94), uri(nodes, 5), hello, graph));
  assert(
    !serd_model_add(model, uri(nodes, 94), uri(nodes, 5), hello_gb, graph));

  // (92 6 "hello"@en-us) and (92 6 "hello"@en-gb)
  assert(
    !serd_model_add(model, uri(nodes, 92), uri(nodes, 6), hello_us, graph));
  assert(
    !serd_model_add(model, uri(nodes, 92), uri(nodes, 6), hello_gb, graph));

  // (14 6 "bonjour"@fr) and (14 6 "salut"@fr)

  const SerdNode* const bonjour =
    serd_nodes_get(nodes, serd_a_plain_literal(zix_string("bonjour"), fr));

  const SerdNode* const salut =
    serd_nodes_get(nodes, serd_a_plain_literal(zix_string("salut"), fr));

  assert(!serd_model_add(model, uri(nodes, 14), uri(nodes, 6), bonjour, graph));
  assert(!serd_model_add(model, uri(nodes, 14), uri(nodes, 6), salut, graph));

  // Attempt to add duplicates
  assert(serd_model_add(model, uri(nodes, 14), uri(nodes, 6), salut, graph));

  // Add a blank node subject
  const SerdNode* const ablank =
    serd_nodes_get(nodes, serd_a_blank(zix_string("ablank")));

  assert(!serd_model_add(model, ablank, uri(nodes, 6), salut, graph));

  // Add statement with URI object
  assert(!serd_model_add(model, ablank, uri(nodes, 6), uri(nodes, 7), graph));

  return EXIT_SUCCESS;
}

static bool
statement_view_equals(const SerdStatementView lhs, const SerdStatementView rhs)
{
  return (lhs.subject == rhs.subject) && (lhs.predicate == rhs.predicate) &&
         (lhs.object == rhs.object) && (lhs.graph == rhs.graph);
}

static inline bool
node_matches(const SerdNode* const ZIX_NULLABLE a,
             const SerdNode* const ZIX_NULLABLE b)
{
  return !a || !b || serd_node_equals(a, b);
}

static bool
statement_view_matches(const SerdStatementView statement,
                       const SerdNode* const   subject,
                       const SerdNode* const   predicate,
                       const SerdNode* const   object,
                       const SerdNode* const   graph)
{
  return (node_matches(statement.subject, subject) &&
          node_matches(statement.predicate, predicate) &&
          node_matches(statement.object, object) &&
          node_matches(statement.graph, graph));
}

static int
test_read(SerdModel* const      model,
          const SerdNode* const g,
          const unsigned        n_quads)
{
  ZixAllocator* const allocator = zix_default_allocator();
  SerdNodes* const    nodes     = serd_nodes_new(allocator);

  SerdCursor* const cursor = serd_model_begin(NULL, model);
  SerdStatementView prev   = {NULL, NULL, NULL, NULL, {NULL, 0, 0}};
  for (; !serd_cursor_equals(cursor, serd_model_end(model));
       serd_cursor_advance(cursor)) {
    const SerdStatementView statement = serd_cursor_get(cursor);
    assert(statement.subject);
    assert(statement.predicate);
    assert(statement.object);
    assert(!statement_view_equals(statement, prev));
    assert(!statement_view_equals(prev, statement));
    prev = statement;
  }

  // Attempt to increment past end
  assert(serd_cursor_advance(cursor) == SERD_BAD_CURSOR);
  serd_cursor_free(NULL, cursor);

  static const ZixStringView s = ZIX_STATIC_STRING("hello");

  const SerdNode* const en_gb = serd_nodes_get(nodes, serd_a_string("en-gb"));
  const SerdNode* const en_us = serd_nodes_get(nodes, serd_a_string("en-us"));

  const SerdNode* const plain_hello =
    serd_nodes_get(nodes, serd_a_string_view(s));

  const SerdNode* const type4_hello =
    serd_nodes_get(nodes, serd_a_typed_literal(s, uri(nodes, 4)));

  const SerdNode* const type5_hello =
    serd_nodes_get(nodes, serd_a_typed_literal(s, uri(nodes, 5)));

  const SerdNode* const gb_hello =
    serd_nodes_get(nodes, serd_a_plain_literal(s, en_gb));

  const SerdNode* const us_hello =
    serd_nodes_get(nodes, serd_a_plain_literal(s, en_us));

#define NUM_PATTERNS 18

  QueryTest patterns[NUM_PATTERNS] = {
    {{NULL, NULL, NULL}, (n_quads * N_OBJECTS_PER) + 12U},
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

  Quad match = {uri(nodes, 1), uri(nodes, 2), uri(nodes, 4), g};
  assert(serd_model_ask(model, match[0], match[1], match[2], match[3]));

  Quad nomatch = {uri(nodes, 1), uri(nodes, 2), uri(nodes, 9), g};
  assert(
    !serd_model_ask(model, nomatch[0], nomatch[1], nomatch[2], nomatch[3]));

  assert(!serd_model_get(model, NULL, NULL, uri(nodes, 3), g));
  assert(!serd_model_get(model, uri(nodes, 1), uri(nodes, 99), NULL, g));

  assert(serd_node_equals(
    serd_model_get(model, uri(nodes, 1), uri(nodes, 2), NULL, g),
    uri(nodes, 3)));
  assert(serd_node_equals(
    serd_model_get(model, uri(nodes, 1), NULL, uri(nodes, 3), g),
    uri(nodes, 2)));
  assert(serd_node_equals(
    serd_model_get(model, NULL, uri(nodes, 2), uri(nodes, 3), g),
    uri(nodes, 1)));
  if (g) {
    assert(serd_node_equals(
      serd_model_get(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), NULL),
      g));
  }

  for (unsigned i = 0; i < NUM_PATTERNS; ++i) {
    QueryTest test = patterns[i];
    Quad      pat  = {test.query[0], test.query[1], test.query[2], g};

    SerdCursor* const range =
      serd_model_find(NULL, model, pat[0], pat[1], pat[2], pat[3]);

    unsigned num_results = 0U;
    for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
      ++num_results;

      const SerdStatementView first = serd_cursor_get(range);
      assert(first.subject);
      assert(first.predicate);
      assert(first.object);
      assert(node_matches(first.subject, pat[0]));
      assert(node_matches(first.predicate, pat[1]));
      assert(node_matches(first.object, pat[2]));
      assert(node_matches(first.graph, pat[3]));
    }

    serd_cursor_free(NULL, range);

    assert(num_results == test.expected_num_results);
  }

#undef NUM_PATTERNS

  // Query blank node subject

  const SerdNode* const ablank =
    serd_nodes_get(nodes, serd_a_blank(zix_string("ablank")));

  Quad        pat         = {ablank, 0, 0};
  unsigned    num_results = 0U;
  SerdCursor* range =
    serd_model_find(NULL, model, pat[0], pat[1], pat[2], pat[3]);

  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    ++num_results;
    const SerdStatementView statement = serd_cursor_get(range);
    assert(statement.subject);
    assert(statement.predicate);
    assert(statement.object);
    assert(node_matches(statement.subject, pat[0]));
    assert(node_matches(statement.predicate, pat[1]));
    assert(node_matches(statement.object, pat[2]));
    assert(node_matches(statement.graph, pat[3]));
  }
  serd_cursor_free(NULL, range);

  assert(num_results == 2U);

  // Test nested queries
  const SerdNode* last_subject = 0;
  range = serd_model_find(NULL, model, NULL, NULL, NULL, NULL);
  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    const SerdStatementView statement = serd_cursor_get(range);
    const SerdNode*         subject   = statement.subject;
    if (subject == last_subject) {
      continue;
    }

    Quad              subpat = {subject, 0, 0};
    SerdCursor* const subrange =
      serd_model_find(NULL, model, subpat[0], subpat[1], subpat[2], subpat[3]);

    assert(subrange);

    const SerdStatementView substatement    = serd_cursor_get(subrange);
    uint64_t                num_sub_results = 0;
    assert(substatement.subject == subject);
    for (; !serd_cursor_is_end(subrange); serd_cursor_advance(subrange)) {
      const SerdStatementView front = serd_cursor_get(subrange);
      assert(front.subject);

      assert(statement_view_matches(
        front, subpat[0], subpat[1], subpat[2], subpat[3]));

      ++num_sub_results;
    }
    serd_cursor_free(NULL, subrange);
    assert(num_sub_results == N_OBJECTS_PER);

    uint64_t count = serd_model_count(model, subject, 0, 0, 0);
    assert(count == num_sub_results);

    last_subject = subject;
  }
  serd_cursor_free(NULL, range);

  serd_nodes_free(nodes);
  return 0;
}

static SerdStatus
expected_error(void* const               handle,
               const SerdLogLevel        level,
               const size_t              n_fields,
               const SerdLogField* const fields,
               const ZixStringView       message)
{
  (void)level;
  (void)n_fields;
  (void)fields;
  (void)handle;

  fprintf(stderr, "expected: %s\n", message.data);
  return SERD_SUCCESS;
}

ZIX_PURE_FUNC static SerdStatus
ignore_only_index_error(void* const               handle,
                        const SerdLogLevel        level,
                        const size_t              n_fields,
                        const SerdLogField* const fields,
                        const ZixStringView       message)
{
  (void)handle;
  (void)level;
  (void)n_fields;
  (void)fields;

  const bool is_index_error = strstr(message.data, "index");

  assert(is_index_error);

  return is_index_error ? SERD_SUCCESS : SERD_UNKNOWN_ERROR;
}

static int
test_failed_new_alloc(SerdWorld* const ignored_world, const unsigned n_quads)
{
  (void)ignored_world;
  (void)n_quads;

  SerdFailingAllocator allocator      = serd_failing_allocator();
  SerdWorld* const     world          = serd_world_new(&allocator.base);
  const size_t         n_world_allocs = allocator.n_allocations;

  // Successfully allocate a model to count the number of allocations
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(model);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_world_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_model_new(world, SERD_ORDER_SPO, 0U));
  }

  serd_model_free(model);
  serd_world_free(world);
  return 0;
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
  assert(serd_model_world(model) == world);
  serd_model_free(model);
  return 0;
}

static int
test_get_default_order(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdModel* const model1 = serd_model_new(world, SERD_ORDER_SPO, 0U);
  SerdModel* const model2 = serd_model_new(world, SERD_ORDER_GPSO, 0U);

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

  const SerdModelFlags flags = SERD_STORE_GRAPHS | SERD_STORE_CARETS;
  SerdModel*           model = serd_model_new(world, SERD_ORDER_SPO, flags);

  assert(serd_model_flags(model) & SERD_STORE_GRAPHS);
  assert(serd_model_flags(model) & SERD_STORE_CARETS);
  serd_model_free(model);
  return 0;
}

static int
test_all_begin(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdModel*  model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  SerdCursor* begin = serd_model_begin(NULL, model);
  SerdCursor* first = serd_model_find(NULL, model, NULL, NULL, NULL, NULL);

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

  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_GRAPHS);

  assert(
    !serd_model_add(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0));

  SerdCursor* i = serd_model_begin_ordered(NULL, model, SERD_ORDER_SPO);
  assert(i);
  assert(!serd_cursor_is_end(i));
  serd_cursor_free(NULL, i);

  i = serd_model_begin_ordered(NULL, model, SERD_ORDER_POS);
  assert(serd_cursor_is_end(i));
  serd_cursor_free(NULL, i);

  serd_model_free(model);
  return 0;
}

static int
test_add_with_iterator(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel*       model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  serd_set_log_func(world, expected_error, NULL);
  assert(
    !serd_model_add(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0));

  // Add a statement with an active iterator
  SerdCursor* iter = serd_model_begin(NULL, model);
  assert(
    !serd_model_add(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 4), 0));

  // Check that iterator has been invalidated
  assert(!serd_cursor_get(iter).subject);
  assert(serd_cursor_advance(iter) == SERD_BAD_CURSOR);

  serd_cursor_free(NULL, iter);
  serd_model_free(model);
  return 0;
}

static int
test_add_remove_nodes(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel*       model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  assert(serd_model_nodes(model));
  assert(serd_nodes_size(serd_model_nodes(model)) == 0);

  const SerdNode* const a = uri(nodes, 1);
  const SerdNode* const b = uri(nodes, 2);
  const SerdNode* const c = uri(nodes, 3);

  // Add 2 statements with 3 nodes
  assert(!serd_model_add(model, a, b, a, NULL));
  assert(!serd_model_add(model, c, b, c, NULL));
  assert(serd_model_size(model) == 2);
  assert(serd_nodes_size(serd_model_nodes(model)) == 3);

  // Remove one statement to leave 2 nodes
  SerdCursor* const begin = serd_model_begin(NULL, model);
  assert(!serd_model_erase(model, begin));
  assert(serd_model_size(model) == 1);
  assert(serd_nodes_size(serd_model_nodes(model)) == 2);
  serd_cursor_free(NULL, begin);

  // Clear the last statement to leave 0 nodes
  assert(!serd_model_clear(model));
  assert(serd_nodes_size(serd_model_nodes(model)) == 0);

  serd_model_free(model);
  return 0;
}

static int
test_add_index(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const      nodes = serd_world_nodes(world);
  SerdModel* const      model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  const SerdNode* const s     = uri(nodes, 0);
  const SerdNode* const p     = uri(nodes, 1);
  const SerdNode* const o1    = uri(nodes, 2);
  const SerdNode* const o2    = uri(nodes, 3);

  // Try to add an existing index
  assert(serd_model_add_index(model, SERD_ORDER_SPO) == SERD_FAILURE);

  // Add a couple of statements
  serd_model_add(model, s, p, o1, NULL);
  serd_model_add(model, s, p, o2, NULL);
  assert(serd_model_size(model) == 2);

  // Add a new index
  assert(!serd_model_add_index(model, SERD_ORDER_PSO));

  // Count statements via the new index
  size_t      count = 0U;
  SerdCursor* cur   = serd_model_find(NULL, model, NULL, p, NULL, NULL);
  while (!serd_cursor_is_end(cur)) {
    ++count;
    serd_cursor_advance(cur);
  }
  serd_cursor_free(NULL, cur);

  serd_model_free(model);
  assert(count == 2);
  return 0;
}

static int
test_remove_index(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const      nodes = serd_world_nodes(world);
  SerdModel* const      model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  const SerdNode* const s     = uri(nodes, 0);
  const SerdNode* const p     = uri(nodes, 1);
  const SerdNode* const o1    = uri(nodes, 2);
  const SerdNode* const o2    = uri(nodes, 3);

  // Try to remove default and non-existent indices
  assert(serd_model_drop_index(model, SERD_ORDER_SPO) == SERD_BAD_CALL);
  assert(serd_model_drop_index(model, SERD_ORDER_PSO) == SERD_FAILURE);

  // Add a couple of statements so that dropping an index isn't trivial
  serd_model_add(model, s, p, o1, NULL);
  serd_model_add(model, s, p, o2, NULL);
  assert(serd_model_size(model) == 2);

  assert(serd_model_add_index(model, SERD_ORDER_PSO) == SERD_SUCCESS);
  assert(serd_model_drop_index(model, SERD_ORDER_PSO) == SERD_SUCCESS);
  assert(serd_model_drop_index(model, SERD_ORDER_PSO) == SERD_FAILURE);
  assert(serd_model_size(model) == 2);
  serd_model_free(model);
  return 0;
}

static int
test_inserter(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const allocator = zix_default_allocator();
  SerdNodes* const    nodes     = serd_nodes_new(allocator);
  SerdModel* const    model     = serd_model_new(world, SERD_ORDER_SPO, 0U);
  SerdSink* const     inserter  = serd_inserter_new(model, NULL);

  const SerdNode* const s =
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/s"));

  const SerdNode* const p =
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/p"));

  const SerdNode* const rel = serd_nodes_get(nodes, serd_a_uri_string("rel"));

  serd_set_log_func(world, expected_error, NULL);

  assert(serd_sink_write(inserter, 0, s, p, rel, NULL) == SERD_BAD_DATA);

  serd_sink_free(inserter);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_erase_with_iterator(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  serd_set_log_func(world, expected_error, NULL);
  assert(
    !serd_model_add(model, uri(nodes, 1), uri(nodes, 2), uri(nodes, 3), 0));
  assert(
    !serd_model_add(model, uri(nodes, 4), uri(nodes, 5), uri(nodes, 6), 0));

  // Erase a statement with an active iterator
  SerdCursor* iter1 = serd_model_begin(NULL, model);
  SerdCursor* iter2 = serd_model_begin(NULL, model);
  assert(!serd_model_erase(model, iter1));

  // Check that erased iterator points to the next statement
  const SerdStatementView s1 = serd_cursor_get(iter1);
  assert(
    statement_view_matches(s1, uri(nodes, 4), uri(nodes, 5), uri(nodes, 6), 0));

  // Check that other iterator has been invalidated
  assert(!serd_cursor_get(iter2).subject);
  assert(serd_cursor_advance(iter2) == SERD_BAD_CURSOR);

  // Check that erasing the end iterator does nothing
  SerdCursor* const end =
    serd_cursor_copy(serd_world_allocator(world), serd_model_end(model));

  assert(serd_model_erase(model, end) == SERD_FAILURE);

  serd_cursor_free(NULL, end);
  serd_cursor_free(NULL, iter2);
  serd_cursor_free(NULL, iter1);
  serd_model_free(model);
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
  const SerdNode* s     = uri(nodes, 1);
  const SerdNode* p     = uri(nodes, 2);
  const SerdNode* hello = serd_nodes_get(nodes, serd_a_string("hello"));

  assert(!serd_model_add(model, s, p, hello, NULL));
  assert(serd_model_ask(model, s, p, hello, NULL));

  // Add (s p "hi")
  const SerdNode* hi = serd_nodes_get(nodes, serd_a_string("hi"));
  assert(!serd_model_add(model, s, p, hi, NULL));
  assert(serd_model_ask(model, s, p, hi, NULL));

  // Erase (s p "hi")
  SerdCursor* iter = serd_model_find(NULL, model, s, p, hi, NULL);
  assert(iter);
  assert(!serd_model_erase(model, iter));
  assert(serd_model_size(model) == 1);
  serd_cursor_free(NULL, iter);

  // Check that erased statement can not be found
  SerdCursor* empty = serd_model_find(NULL, model, s, p, hi, NULL);
  assert(serd_cursor_is_end(empty));
  serd_cursor_free(NULL, empty);

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_bad_statement(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const allocator = serd_world_allocator(world);
  SerdNodes* const    nodes     = serd_nodes_new(allocator);

  const SerdNode* s = serd_nodes_get(nodes, serd_a_uri_string("urn:s"));
  const SerdNode* p = serd_nodes_get(nodes, serd_a_uri_string("urn:p"));
  const SerdNode* o = serd_nodes_get(nodes, serd_a_uri_string("urn:o"));

  const SerdNode* f =
    serd_nodes_get(nodes, serd_a_uri_string("file:///tmp/file.ttl"));

  const SerdCaretView caret = {f, 16, 18};

  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  assert(!serd_model_add_from(model, s, p, o, NULL, caret));

  SerdCursor* const       begin     = serd_model_begin(NULL, model);
  const SerdStatementView statement = serd_cursor_get(begin);

  assert(serd_node_equals(statement.subject, s));
  assert(serd_node_equals(statement.predicate, p));
  assert(serd_node_equals(statement.object, o));
  assert(!statement.graph);

  assert(serd_node_equals(statement.caret.document, f));
  assert(statement.caret.line == 16);
  assert(statement.caret.column == 18);

  assert(!serd_model_erase(model, begin));

  serd_cursor_free(NULL, begin);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_add_with_caret(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_nodes_new(serd_world_allocator(world));
  const SerdNode*  lit   = serd_nodes_get(nodes, serd_a_string("string"));
  const SerdNode*  uri   = serd_nodes_get(nodes, serd_a_uri_string("urn:uri"));

  const SerdNode* blank = serd_nodes_get(nodes, serd_a_blank(zix_string("b1")));

  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  assert(serd_model_add(model, lit, uri, uri, NULL));
  assert(serd_model_add(model, uri, blank, uri, NULL));
  assert(serd_model_add(model, uri, uri, uri, lit));

  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_erase_all(SerdWorld* const world, const unsigned n_quads)
{
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  serd_model_add_index(model, SERD_ORDER_OSP);
  generate(world, model, n_quads, NULL);

  SerdCursor* iter = serd_model_begin(NULL, model);
  while (!serd_cursor_equals(iter, serd_model_end(model))) {
    assert(!serd_model_erase(model, iter));
  }

  assert(serd_model_empty(model));

  serd_cursor_free(NULL, iter);
  serd_model_free(model);
  return 0;
}

static int
test_clear(SerdWorld* const world, const unsigned n_quads)
{
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(world, model, n_quads, NULL);

  serd_model_clear(model);
  assert(serd_model_empty(model));

  serd_model_free(model);
  return 0;
}

static int
test_copy(SerdWorld* const world, const unsigned n_quads)
{
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(world, model, n_quads, NULL);

  SerdModel* copy = serd_model_copy(serd_world_allocator(world), model);
  assert(serd_model_equals(model, copy));

  serd_model_free(model);
  serd_model_free(copy);
  return 0;
}

static int
test_equals(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(world, model, n_quads, NULL);
  serd_model_add(
    model, uri(nodes, 0), uri(nodes, 1), uri(nodes, 2), uri(nodes, 3));

  assert(serd_model_equals(NULL, NULL));
  assert(!serd_model_equals(NULL, model));
  assert(!serd_model_equals(model, NULL));

  SerdModel* empty = serd_model_new(world, SERD_ORDER_SPO, 0U);
  assert(!serd_model_equals(model, empty));

  SerdModel* different = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(world, different, n_quads, NULL);
  serd_model_add(
    different, uri(nodes, 1), uri(nodes, 1), uri(nodes, 2), uri(nodes, 3));

  assert(serd_model_size(model) == serd_model_size(different));
  assert(!serd_model_equals(model, different));

  serd_model_free(model);
  serd_model_free(empty);
  serd_model_free(different);
  return 0;
}

static int
test_find_past_end(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  const SerdNode*  s     = uri(nodes, 1);
  const SerdNode*  p     = uri(nodes, 2);
  const SerdNode*  o     = uri(nodes, 3);
  assert(!serd_model_add(model, s, p, o, 0));
  assert(serd_model_ask(model, s, p, o, 0));

  const SerdNode* huge  = uri(nodes, 999);
  SerdCursor*     range = serd_model_find(NULL, model, huge, huge, huge, 0);
  assert(serd_cursor_is_end(range));

  serd_cursor_free(NULL, range);
  serd_model_free(model);
  return 0;
}

static int
test_find_unknown_node(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);

  const SerdNode* const s = uri(nodes, 1);
  const SerdNode* const p = uri(nodes, 2);
  const SerdNode* const o = uri(nodes, 3);

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_GRAPHS);

  // Add one statement
  assert(!serd_model_add(model, s, p, o, NULL));
  assert(serd_model_ask(model, s, p, o, NULL));

  /* Test searching for statements that contain a non-existent node.  This is
     semantically equivalent to any other non-matching pattern, but can be
     implemented with a fast path that avoids searching a statement index
     entirely. */

  const SerdNode* const q = uri(nodes, 42);
  assert(!serd_model_ask(model, s, p, o, q));
  assert(!serd_model_ask(model, s, p, q, NULL));
  assert(!serd_model_ask(model, s, q, o, NULL));
  assert(!serd_model_ask(model, q, p, o, NULL));

  serd_model_free(model);
  return 0;
}

static int
test_find_graph(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);

  const SerdNode* const s  = uri(nodes, 1);
  const SerdNode* const p  = uri(nodes, 2);
  const SerdNode* const o1 = uri(nodes, 3);
  const SerdNode* const o2 = uri(nodes, 4);
  const SerdNode* const g  = uri(nodes, 5);

  for (unsigned indexed = 0U; indexed < 2U; ++indexed) {
    SerdModel* const model =
      serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_GRAPHS);

    if (indexed) {
      serd_model_add_index(model, SERD_ORDER_GSPO);
    }

    // Add one statement in a named graph and one in the default graph
    assert(!serd_model_add(model, s, p, o1, NULL));
    assert(!serd_model_add(model, s, p, o2, g));

    // Both statements can be found in the default graph
    assert(serd_model_ask(model, s, p, o1, NULL));
    assert(serd_model_ask(model, s, p, o2, NULL));

    // Only the one statement can be found in the named graph
    assert(!serd_model_ask(model, s, p, o1, g));
    assert(serd_model_ask(model, s, p, o2, g));

    serd_model_free(model);
  }

  return 0;
}

static int
test_range(SerdWorld* const world, const unsigned n_quads)
{
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  generate(world, model, n_quads, NULL);

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

  return 0;
}

static int
test_triple_index_read(SerdWorld* const world, const unsigned n_quads)
{
  serd_set_log_func(world, ignore_only_index_error, NULL);

  for (unsigned i = 0; i < 6; ++i) {
    SerdModel* const model = serd_model_new(world, (SerdStatementOrder)i, 0U);
    generate(world, model, n_quads, 0);
    assert(!test_read(model, 0, n_quads));
    serd_model_free(model);
  }

  return 0;
}

static int
test_quad_index_read(SerdWorld* const world, const unsigned n_quads)
{
  SerdNodes* const nodes = serd_world_nodes(world);

  serd_set_log_func(world, ignore_only_index_error, NULL);

  for (unsigned i = 0; i < 6; ++i) {
    SerdModel* const model =
      serd_model_new(world, (SerdStatementOrder)i, SERD_STORE_GRAPHS);

    const SerdNode* graph = uri(nodes, 42);
    generate(world, model, n_quads, graph);
    assert(!test_read(model, graph, n_quads));
    serd_model_free(model);
  }

  return 0;
}

static int
test_remove_graph(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_GSPO, SERD_STORE_GRAPHS);

  // Generate a couple of graphs
  const SerdNode* graph42 = uri(nodes, 42);
  const SerdNode* graph43 = uri(nodes, 43);
  generate(world, model, 1, graph42);
  generate(world, model, 1, graph43);

  // Find the start of graph43
  SerdCursor* range = serd_model_find(NULL, model, NULL, NULL, NULL, graph43);
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
  Quad pat = {0, 0, 0, graph42};
  for (iter = serd_model_begin(NULL, model);
       !serd_cursor_equals(iter, serd_model_end(model));
       serd_cursor_advance(iter)) {
    const SerdStatementView s = serd_cursor_get(iter);
    assert(statement_view_matches(s, pat[0], pat[1], pat[2], pat[3]));
  }
  serd_cursor_free(NULL, iter);

  serd_model_free(model);
  return 0;
}

static int
test_default_graph(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  SerdNodes* const nodes = serd_world_nodes(world);

  const SerdNode* s  = uri(nodes, 1);
  const SerdNode* p  = uri(nodes, 2);
  const SerdNode* o  = uri(nodes, 3);
  const SerdNode* g1 = uri(nodes, 101);
  const SerdNode* g2 = uri(nodes, 102);

  {
    // Make a model that does not store graphs
    SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

    // Insert a statement into a graph (which will be dropped)
    assert(!serd_model_add(model, s, p, o, g1));

    // Attempt to insert the same statement into another graph
    assert(serd_model_add(model, s, p, o, g2) == SERD_FAILURE);

    // Ensure that we only see the statement once
    assert(serd_model_count(model, s, p, o, NULL) == 1);

    serd_model_free(model);
  }

  {
    // Make a model that stores graphs
    SerdModel* const model =
      serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_GRAPHS);

    // Insert the same statement into two graphs
    assert(!serd_model_add(model, s, p, o, g1));
    assert(!serd_model_add(model, s, p, o, g2));

    // Ensure we see the statement twice
    assert(serd_model_count(model, s, p, o, NULL) == 2);

    serd_model_free(model);
  }

  return 0;
}

static int
test_write_flat_range(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const alloc = serd_world_allocator(world);

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_GRAPHS);
  SerdNodes* nodes = serd_nodes_new(alloc);

  const SerdNode* s  = serd_nodes_get(nodes, serd_a_uri_string("urn:s"));
  const SerdNode* p  = serd_nodes_get(nodes, serd_a_uri_string("urn:p"));
  const SerdNode* b1 = serd_nodes_get(nodes, serd_a_blank(zix_string("b1")));
  const SerdNode* b2 = serd_nodes_get(nodes, serd_a_blank(zix_string("b2")));
  const SerdNode* o  = serd_nodes_get(nodes, serd_a_uri_string("urn:o"));

  serd_model_add(model, s, p, b1, NULL);
  serd_model_add(model, b1, p, o, NULL);
  serd_model_add(model, s, p, b2, NULL);
  serd_model_add(model, b2, p, o, NULL);

  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdEnv*         env    = serd_env_new(alloc, zix_empty_string());
  SerdOutputStream out    = serd_open_output_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, &out, 1);
  assert(writer);

  SerdCursor* const all = serd_model_begin(NULL, model);
  while (!serd_cursor_is_end(all)) {
    serd_sink_write_statement(
      serd_writer_sink(writer), 0U, serd_cursor_get(all));
    serd_cursor_advance(all);
  }
  serd_cursor_free(NULL, all);

  serd_writer_finish(writer);
  serd_close_output(&out);

  const char* const        str      = (const char*)buffer.buf;
  static const char* const expected = "<urn:s>\n"
                                      "\t<urn:p> _:b1 ,\n"
                                      "\t\t_:b2 .\n"
                                      "\n"
                                      "_:b1\n"
                                      "\t<urn:p> <urn:o> .\n"
                                      "\n"
                                      "_:b2\n"
                                      "\t<urn:p> <urn:o> .\n";

  assert(str);
  assert(!strcmp(str, expected));

  zix_free(buffer.allocator, buffer.buf);
  serd_writer_free(writer);
  serd_model_free(model);
  serd_env_free(env);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_write_bad_list(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const alloc = serd_world_allocator(world);

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_GRAPHS);
  SerdNodes* nodes = serd_nodes_new(alloc);

  serd_model_add_index(model, SERD_ORDER_OPS);

  const SerdNode* s = serd_nodes_get(nodes, serd_a_uri_string("urn:s"));
  const SerdNode* p = serd_nodes_get(nodes, serd_a_uri_string("urn:p"));

  const SerdNode* list1 = serd_nodes_get(nodes, serd_a_blank(zix_string("l1")));

  const SerdNode* list2 = serd_nodes_get(nodes, serd_a_blank(zix_string("l2")));

  const SerdNode* nofirst =
    serd_nodes_get(nodes, serd_a_blank(zix_string("nof")));

  const SerdNode* norest =
    serd_nodes_get(nodes, serd_a_blank(zix_string("nor")));

  const SerdNode* pfirst = serd_nodes_get(nodes, serd_a_uri_string(RDF_FIRST));
  const SerdNode* prest  = serd_nodes_get(nodes, serd_a_uri_string(RDF_REST));

  const SerdNode* val1 = serd_nodes_get(nodes, serd_a_string("a"));
  const SerdNode* val2 = serd_nodes_get(nodes, serd_a_string("b"));

  // List where second node has no rdf:first
  serd_model_add(model, s, p, list1, NULL);
  serd_model_add(model, list1, pfirst, val1, NULL);
  serd_model_add(model, list1, prest, nofirst, NULL);

  // List where second node has no rdf:rest
  serd_model_add(model, s, p, list2, NULL);
  serd_model_add(model, list2, pfirst, val1, NULL);
  serd_model_add(model, list2, prest, norest, NULL);
  serd_model_add(model, norest, pfirst, val2, NULL);

  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdEnv*         env    = serd_env_new(alloc, zix_empty_string());
  SerdOutputStream out    = serd_open_output_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, &out, 1);
  assert(writer);

  SerdCursor* all = serd_model_begin(NULL, model);
  serd_describe_range(NULL, all, serd_writer_sink(writer), 0);
  serd_cursor_free(NULL, all);

  serd_writer_finish(writer);
  serd_close_output(&out);

  const char* str      = (const char*)buffer.buf;
  const char* expected = "<urn:s>\n"
                         "	<urn:p> (\n"
                         "		\"a\"\n"
                         "	) , (\n"
                         "		\"a\"\n"
                         "		\"b\"\n"
                         "	) .\n";

  assert(str);
  assert(!strcmp(str, expected));

  zix_free(buffer.allocator, buffer.buf);
  serd_writer_free(writer);
  serd_close_output(&out);
  serd_model_free(model);
  serd_env_free(env);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_write_infinite_list(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const alloc = serd_world_allocator(world);

  SerdModel* const model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_GRAPHS);
  SerdNodes* nodes = serd_nodes_new(alloc);

  serd_model_add_index(model, SERD_ORDER_OPS);

  const SerdNode* s = serd_nodes_get(nodes, serd_a_uri_string("urn:s"));
  const SerdNode* p = serd_nodes_get(nodes, serd_a_uri_string("urn:p"));

  const SerdNode* list1 = serd_nodes_get(nodes, serd_a_blank(zix_string("l1")));

  const SerdNode* list2 = serd_nodes_get(nodes, serd_a_blank(zix_string("l2")));

  const SerdNode* pfirst = serd_nodes_get(nodes, serd_a_uri_string(RDF_FIRST));
  const SerdNode* prest  = serd_nodes_get(nodes, serd_a_uri_string(RDF_REST));
  const SerdNode* val1   = serd_nodes_get(nodes, serd_a_string("a"));
  const SerdNode* val2   = serd_nodes_get(nodes, serd_a_string("b"));

  // List with a cycle: list1 -> list2 -> list1 -> list2 ...
  serd_model_add(model, s, p, list1, NULL);
  serd_model_add(model, list1, pfirst, val1, NULL);
  serd_model_add(model, list1, prest, list2, NULL);
  serd_model_add(model, list2, pfirst, val2, NULL);
  serd_model_add(model, list2, prest, list1, NULL);

  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdEnv*         env    = serd_env_new(alloc, zix_empty_string());
  SerdOutputStream out    = serd_open_output_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, &out, 1);
  assert(writer);

  serd_env_set_prefix(
    env,
    zix_string("rdf"),
    zix_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#"));

  SerdCursor* all = serd_model_begin(NULL, model);
  serd_describe_range(NULL, all, serd_writer_sink(writer), 0);
  serd_cursor_free(NULL, all);

  serd_writer_finish(writer);
  serd_close_output(&out);
  const char* str      = (const char*)buffer.buf;
  const char* expected = "<urn:s>\n"
                         "	<urn:p> _:l1 .\n"
                         "\n"
                         "_:l1\n"
                         "	rdf:first \"a\" ;\n"
                         "	rdf:rest [\n"
                         "		rdf:first \"b\" ;\n"
                         "		rdf:rest _:l1\n"
                         "	] .\n";

  assert(str);
  assert(!strcmp(str, expected));

  zix_free(buffer.allocator, buffer.buf);
  serd_writer_free(writer);
  serd_close_output(&out);
  serd_model_free(model);
  serd_env_free(env);
  serd_nodes_free(nodes);

  return 0;
}

typedef struct {
  size_t n_written;
  size_t max_successes;
} FailingWriteFuncState;

/// Write function that fails after a certain number of writes
static SerdStreamResult
failing_write_func(void* const stream, const size_t len, const void* buf)
{
  (void)buf;

  FailingWriteFuncState* state = (FailingWriteFuncState*)stream;

  if (++state->n_written > state->max_successes) {
    const SerdStreamResult r = {SERD_BAD_WRITE, 0U};
    return r;
  }

  const SerdStreamResult r = {SERD_SUCCESS, len};
  return r;
}

static int
test_write_error_in_list_subject(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const alloc = serd_world_allocator(world);

  serd_set_log_func(world, expected_error, NULL);

  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  SerdNodes*       nodes = serd_nodes_new(alloc);

  serd_model_add_index(model, SERD_ORDER_OPS);

  const SerdNode* p   = serd_nodes_get(nodes, serd_a_uri_string("urn:p"));
  const SerdNode* o   = serd_nodes_get(nodes, serd_a_uri_string("urn:o"));
  const SerdNode* l1  = serd_nodes_get(nodes, serd_a_blank(zix_string("l1")));
  const SerdNode* one = serd_nodes_get(nodes, serd_a_integer(1));
  const SerdNode* l2  = serd_nodes_get(nodes, serd_a_blank(zix_string("l2")));
  const SerdNode* two = serd_nodes_get(nodes, serd_a_integer(2));

  const SerdNode* rdf_first =
    serd_nodes_get(nodes, serd_a_uri_string(RDF_FIRST));

  const SerdNode* rdf_rest = serd_nodes_get(nodes, serd_a_uri_string(RDF_REST));

  const SerdNode* rdf_nil = serd_nodes_get(nodes, serd_a_uri_string(RDF_NIL));

  serd_model_add(model, l1, rdf_first, one, NULL);
  serd_model_add(model, l1, rdf_rest, l2, NULL);
  serd_model_add(model, l2, rdf_first, two, NULL);
  serd_model_add(model, l2, rdf_rest, rdf_nil, NULL);
  serd_model_add(model, l1, p, o, NULL);

  SerdEnv* env = serd_env_new(alloc, zix_empty_string());

  for (size_t max_successes = 0; max_successes < 18; ++max_successes) {
    FailingWriteFuncState state = {0, max_successes};
    SerdOutputStream      out =
      serd_open_output_stream(failing_write_func, NULL, &state);

    SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, &out, 1);

    const SerdSink* const sink = serd_writer_sink(writer);
    SerdCursor* const     all  = serd_model_begin(NULL, model);
    const SerdStatus      st   = serd_describe_range(NULL, all, sink, 0);
    serd_cursor_free(NULL, all);

    assert(st == SERD_BAD_WRITE);

    serd_writer_free(writer);
    serd_close_output(&out);
  }

  serd_env_free(env);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_write_error_in_list_object(SerdWorld* const world, const unsigned n_quads)
{
  (void)n_quads;

  ZixAllocator* const alloc = serd_world_allocator(world);

  serd_set_log_func(world, expected_error, NULL);

  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  SerdNodes* const nodes = serd_nodes_new(alloc);

  serd_model_add_index(model, SERD_ORDER_OPS);

  const SerdNode* s   = serd_nodes_get(nodes, serd_a_uri_string("urn:s"));
  const SerdNode* p   = serd_nodes_get(nodes, serd_a_uri_string("urn:p"));
  const SerdNode* l1  = serd_nodes_get(nodes, serd_a_blank(zix_string("l1")));
  const SerdNode* one = serd_nodes_get(nodes, serd_a_integer(1));
  const SerdNode* l2  = serd_nodes_get(nodes, serd_a_blank(zix_string("l2")));
  const SerdNode* two = serd_nodes_get(nodes, serd_a_integer(2));

  const SerdNode* rdf_first =
    serd_nodes_get(nodes, serd_a_uri_string(RDF_FIRST));

  const SerdNode* rdf_rest = serd_nodes_get(nodes, serd_a_uri_string(RDF_REST));

  const SerdNode* rdf_nil = serd_nodes_get(nodes, serd_a_uri_string(RDF_NIL));

  serd_model_add(model, s, p, l1, NULL);
  serd_model_add(model, l1, rdf_first, one, NULL);
  serd_model_add(model, l1, rdf_rest, l2, NULL);
  serd_model_add(model, l2, rdf_first, two, NULL);
  serd_model_add(model, l2, rdf_rest, rdf_nil, NULL);

  SerdEnv* env = serd_env_new(alloc, zix_empty_string());

  for (size_t max_successes = 0; max_successes < 21; ++max_successes) {
    FailingWriteFuncState state = {0, max_successes};
    SerdOutputStream      out =
      serd_open_output_stream(failing_write_func, NULL, &state);

    SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, &out, 1);

    const SerdSink* const sink = serd_writer_sink(writer);
    SerdCursor* const     all  = serd_model_begin(NULL, model);
    const SerdStatus      st   = serd_describe_range(NULL, all, sink, 0);
    serd_cursor_free(NULL, all);

    assert(st == SERD_BAD_WRITE);

    serd_writer_free(writer);
    serd_close_output(&out);
  }

  serd_env_free(env);
  serd_model_free(model);
  serd_nodes_free(nodes);
  return 0;
}

int
main(void)
{
  static const unsigned n_quads = 300;

  serd_model_free(NULL); // Shouldn't crash

  typedef int (*TestFunc)(SerdWorld*, unsigned);

  const TestFunc tests[] = {test_failed_new_alloc,
                            test_free_null,
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
                            test_erase_with_iterator,
                            test_add_erase,
                            test_add_bad_statement,
                            test_add_with_caret,
                            test_erase_all,
                            test_clear,
                            test_copy,
                            test_equals,
                            test_find_past_end,
                            test_find_unknown_node,
                            test_find_graph,
                            test_range,
                            test_triple_index_read,
                            test_quad_index_read,
                            test_remove_graph,
                            test_default_graph,
                            test_write_flat_range,
                            test_write_bad_list,
                            test_write_infinite_list,
                            test_write_error_in_list_subject,
                            test_write_error_in_list_object,
                            NULL};

  SerdWorld* world = serd_world_new(NULL);
  int        ret   = 0;

  for (const TestFunc* t = tests; *t; ++t) {
    serd_set_log_func(world, NULL, NULL);
    ret += (*t)(world, n_quads);
  }

  serd_world_free(world);
  return ret;
}
