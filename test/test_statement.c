// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/caret.h"
#include "serd/field.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/statement.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stddef.h>

#define NS_EG "http://example.org/"

static void
test_new(void)
{
  ZixAllocator* const allocator = zix_default_allocator();
  SerdNodes* const    nodes     = serd_nodes_new(allocator);

  const SerdNode* const u = serd_nodes_get(nodes, serd_a_uri_string(NS_EG "u"));
  const SerdNode* const c = serd_nodes_get(nodes, serd_a_curie_string("eg:c"));
  const SerdNode* const b = serd_nodes_get(nodes, serd_a_blank_string("blank"));
  const SerdNode* const l = serd_nodes_get(nodes, serd_a_string("str"));

  // Anything can be a URI
  {
    SerdStatement* const statement = serd_statement_new(NULL, u, u, u, u, NULL);
    assert(statement);
    serd_statement_free(NULL, statement);
  }

  // P may not be a blank node
  assert(!serd_statement_new(NULL, c, b, u, NULL, NULL));

  // S, P, and G may not be literals (must be resources)
  assert(!serd_statement_new(NULL, l, c, u, u, NULL));
  assert(!serd_statement_new(NULL, u, l, c, u, NULL));
  assert(!serd_statement_new(NULL, b, u, u, l, NULL));

  serd_nodes_free(nodes);
}

static void
test_new_failed_alloc(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNode* const u = serd_nodes_get(nodes, serd_a_uri_string(NS_EG "u"));
  const SerdNode* const doc =
    serd_nodes_get(nodes, serd_a_uri_string(NS_EG "document"));

  SerdCaret* const caret = serd_caret_new(NULL, doc, 1, 79);

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a new statement to count the number of allocations
  SerdStatement* const statement =
    serd_statement_new(&allocator.base, u, u, u, NULL, caret);
  assert(statement);
  serd_statement_free(&allocator.base, statement);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations;
  for (size_t i = 0U; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_statement_new(&allocator.base, u, u, u, NULL, caret));
  }

  serd_caret_free(NULL, caret);
  serd_nodes_free(nodes);
}

static void
test_copy(void)
{
  assert(!serd_statement_copy(NULL, NULL));

  SerdNode* const s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* const p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode* const o = serd_node_new(NULL, serd_a_uri_string(NS_EG "o"));
  SerdNode* const g = serd_node_new(NULL, serd_a_uri_string(NS_EG "g"));

  SerdStatement* const statement = serd_statement_new(NULL, s, p, o, g, NULL);
  SerdStatement* const copy      = serd_statement_copy(NULL, statement);

  assert(serd_statement_equals(copy, statement));
  assert(!serd_statement_caret(copy));

  serd_statement_free(NULL, copy);
  serd_statement_free(NULL, statement);
  serd_node_free(NULL, g);
  serd_node_free(NULL, o);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
}

static void
test_copy_with_caret(void)
{
  assert(!serd_statement_copy(NULL, NULL));

  SerdNode* const f = serd_node_new(NULL, serd_a_string("file"));
  SerdNode* const s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* const p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode* const o = serd_node_new(NULL, serd_a_uri_string(NS_EG "o"));
  SerdNode* const g = serd_node_new(NULL, serd_a_uri_string(NS_EG "g"));

  SerdCaret* const     caret     = serd_caret_new(NULL, f, 1, 1);
  SerdStatement* const statement = serd_statement_new(NULL, s, p, o, g, caret);
  SerdStatement* const copy      = serd_statement_copy(NULL, statement);

  assert(serd_statement_equals(copy, statement));
  assert(serd_caret_equals(serd_statement_caret(copy), caret));

  serd_statement_free(NULL, copy);
  serd_statement_free(NULL, statement);
  serd_caret_free(NULL, caret);
  serd_node_free(NULL, g);
  serd_node_free(NULL, o);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_node_free(NULL, f);
}

static void
test_copy_failed_alloc(void)
{
  SerdNode* const u = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* const doc =
    serd_node_new(NULL, serd_a_uri_string(NS_EG "document"));
  SerdCaret* const caret = serd_caret_new(NULL, doc, 1, 79);

  SerdStatement* const statement =
    serd_statement_new(NULL, u, u, u, NULL, caret);

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully copy the statement to count the number of allocations
  SerdStatement* const copy = serd_statement_copy(&allocator.base, statement);
  assert(copy);
  serd_statement_free(&allocator.base, copy);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations;
  for (size_t i = 0U; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_statement_copy(&allocator.base, statement));
  }

  serd_statement_free(NULL, statement);
  serd_caret_free(NULL, caret);
  serd_node_free(NULL, doc);
  serd_node_free(NULL, u);
}

static void
test_free(void)
{
  serd_statement_free(zix_default_allocator(), NULL);
  serd_statement_free(NULL, NULL);
}

static void
test_fields(void)
{
  SerdNode* const f = serd_node_new(NULL, serd_a_string("file"));
  SerdNode* const s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* const p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode* const o = serd_node_new(NULL, serd_a_uri_string(NS_EG "o"));
  SerdNode* const g = serd_node_new(NULL, serd_a_uri_string(NS_EG "g"));

  SerdCaret* const caret = serd_caret_new(NULL, f, 1, 1);

  SerdStatement* const statement = serd_statement_new(NULL, s, p, o, g, caret);

  assert(serd_statement_equals(statement, statement));
  assert(!serd_statement_equals(statement, NULL));
  assert(!serd_statement_equals(NULL, statement));

  assert(serd_statement_node(statement, SERD_SUBJECT) == s);
  assert(serd_statement_node(statement, SERD_PREDICATE) == p);
  assert(serd_statement_node(statement, SERD_OBJECT) == o);
  assert(serd_statement_node(statement, SERD_GRAPH) == g);

  assert(serd_statement_subject(statement) == s);
  assert(serd_statement_predicate(statement) == p);
  assert(serd_statement_object(statement) == o);
  assert(serd_statement_graph(statement) == g);
  assert(serd_statement_caret(statement) != caret);
  assert(serd_caret_equals(serd_statement_caret(statement), caret));
  assert(serd_statement_matches(statement, s, p, o, g));
  assert(serd_statement_matches(statement, NULL, p, o, g));
  assert(serd_statement_matches(statement, s, NULL, o, g));
  assert(serd_statement_matches(statement, s, p, NULL, g));
  assert(serd_statement_matches(statement, s, p, o, NULL));
  assert(!serd_statement_matches(statement, o, NULL, NULL, NULL));
  assert(!serd_statement_matches(statement, NULL, o, NULL, NULL));
  assert(!serd_statement_matches(statement, NULL, NULL, s, NULL));
  assert(!serd_statement_matches(statement, NULL, NULL, NULL, s));

  SerdStatement* const diff_s = serd_statement_new(NULL, o, p, o, g, caret);
  assert(!serd_statement_equals(statement, diff_s));
  serd_statement_free(NULL, diff_s);

  SerdStatement* const diff_p = serd_statement_new(NULL, s, o, o, g, caret);
  assert(!serd_statement_equals(statement, diff_p));
  serd_statement_free(NULL, diff_p);

  SerdStatement* const diff_o = serd_statement_new(NULL, s, p, s, g, caret);
  assert(!serd_statement_equals(statement, diff_o));
  serd_statement_free(NULL, diff_o);

  SerdStatement* const diff_g = serd_statement_new(NULL, s, p, o, s, caret);
  assert(!serd_statement_equals(statement, diff_g));
  serd_statement_free(NULL, diff_g);

  serd_statement_free(NULL, statement);
  serd_caret_free(NULL, caret);
  serd_node_free(NULL, g);
  serd_node_free(NULL, o);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_node_free(NULL, f);
}

int
main(void)
{
  test_new();
  test_new_failed_alloc();
  test_copy();
  test_copy_with_caret();
  test_copy_failed_alloc();
  test_free();
  test_fields();

  return 0;
}
