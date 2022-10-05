// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define NS_EG "http://example.org/"

static void
test_invalid_new(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  assert(!serd_statement_copy(allocator, NULL));

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const s = serd_nodes_string(nodes, serd_string("s"));
  const SerdNode* const u = serd_nodes_uri(nodes, serd_string(NS_EG "u"));
  const SerdNode* const b = serd_nodes_blank(nodes, serd_string(NS_EG "b"));

  // S, P, and G may not be strings (must be resources)
  assert(!serd_statement_new(allocator, s, u, u, u, NULL));
  assert(!serd_statement_new(allocator, u, s, u, u, NULL));
  assert(!serd_statement_new(allocator, u, u, u, s, NULL));

  // P may not be a blank node
  assert(!serd_statement_new(allocator, u, b, u, u, NULL));

  serd_nodes_free(nodes);
}

static void
test_copy(void)
{
  assert(!serd_statement_copy(NULL, NULL));

  SerdAllocator* const allocator = serd_default_allocator();

  assert(!serd_statement_copy(allocator, NULL));

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const s = serd_nodes_uri(nodes, serd_string(NS_EG "s"));
  const SerdNode* const p = serd_nodes_uri(nodes, serd_string(NS_EG "p"));
  const SerdNode* const o = serd_nodes_uri(nodes, serd_string(NS_EG "o"));
  const SerdNode* const g = serd_nodes_uri(nodes, serd_string(NS_EG "g"));

  SerdStatement* const statement =
    serd_statement_new(allocator, s, p, o, g, NULL);

  SerdStatement* const copy = serd_statement_copy(allocator, statement);

  assert(serd_statement_equals(copy, statement));

  serd_statement_free(allocator, copy);
  serd_statement_free(allocator, statement);
  serd_nodes_free(nodes);
}

static void
test_copy_with_caret(void)
{
  assert(!serd_statement_copy(NULL, NULL));

  SerdAllocator* const allocator = serd_default_allocator();

  assert(!serd_statement_copy(allocator, NULL));

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const f = serd_nodes_string(nodes, serd_string("file"));
  const SerdNode* const s = serd_nodes_uri(nodes, serd_string(NS_EG "s"));
  const SerdNode* const p = serd_nodes_uri(nodes, serd_string(NS_EG "p"));
  const SerdNode* const o = serd_nodes_uri(nodes, serd_string(NS_EG "o"));
  const SerdNode* const g = serd_nodes_uri(nodes, serd_string(NS_EG "g"));

  SerdCaret* const caret = serd_caret_new(allocator, f, 1, 1);

  SerdStatement* const statement =
    serd_statement_new(allocator, s, p, o, g, caret);

  SerdStatement* const copy = serd_statement_copy(allocator, statement);

  assert(serd_statement_equals(copy, statement));
  assert(serd_caret_equals(serd_statement_caret(copy), caret));

  serd_statement_free(allocator, copy);
  serd_caret_free(allocator, caret);
  serd_statement_free(allocator, statement);
  serd_nodes_free(nodes);
}

static void
test_free(void)
{
  serd_statement_free(serd_default_allocator(), NULL);
  serd_statement_free(NULL, NULL);
}

static void
test_fields(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const f = serd_nodes_string(nodes, serd_string("file"));
  const SerdNode* const s = serd_nodes_uri(nodes, serd_string(NS_EG "s"));
  const SerdNode* const p = serd_nodes_uri(nodes, serd_string(NS_EG "p"));
  const SerdNode* const o = serd_nodes_uri(nodes, serd_string(NS_EG "o"));
  const SerdNode* const g = serd_nodes_uri(nodes, serd_string(NS_EG "g"));

  SerdCaret* const caret = serd_caret_new(allocator, f, 1, 1);

  SerdStatement* const statement =
    serd_statement_new(allocator, s, p, o, g, caret);

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

  SerdStatement* const diff_s =
    serd_statement_new(allocator, o, p, o, g, caret);
  assert(!serd_statement_equals(statement, diff_s));
  serd_statement_free(allocator, diff_s);

  SerdStatement* const diff_p =
    serd_statement_new(allocator, s, o, o, g, caret);
  assert(!serd_statement_equals(statement, diff_p));
  serd_statement_free(allocator, diff_p);

  SerdStatement* const diff_o =
    serd_statement_new(allocator, s, p, s, g, caret);
  assert(!serd_statement_equals(statement, diff_o));
  serd_statement_free(allocator, diff_o);

  SerdStatement* const diff_g =
    serd_statement_new(allocator, s, p, o, s, caret);
  assert(!serd_statement_equals(statement, diff_g));
  serd_statement_free(allocator, diff_g);

  serd_statement_free(allocator, statement);
  serd_caret_free(allocator, caret);
  serd_nodes_free(nodes);
}

static void
test_failed_alloc(void)
{
  SerdNodes* const nodes = serd_nodes_new(serd_default_allocator());

  const SerdNode* const f = serd_nodes_string(nodes, serd_string("file"));
  const SerdNode* const s = serd_nodes_uri(nodes, serd_string(NS_EG "s"));
  const SerdNode* const p = serd_nodes_uri(nodes, serd_string(NS_EG "p"));
  const SerdNode* const o = serd_nodes_uri(nodes, serd_string(NS_EG "o"));
  const SerdNode* const g = serd_nodes_uri(nodes, serd_string(NS_EG "g"));

  SerdCaret* const caret = serd_caret_new(serd_default_allocator(), f, 1, 1);

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a statement to count the number of allocations
  SerdStatement* const statement =
    serd_statement_new(&allocator.base, s, p, o, g, caret);
  assert(statement);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations;
  for (size_t i = 0U; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_statement_new(&allocator.base, s, p, o, g, caret));
  }

  // Successfully copy the statement to count the number of allocations
  allocator.n_allocations   = 0;
  allocator.n_remaining     = SIZE_MAX;
  SerdStatement* const copy = serd_statement_copy(&allocator.base, statement);
  assert(copy);

  // Test that each allocation failing is handled gracefully
  const size_t n_copy_allocs = allocator.n_allocations;
  for (size_t i = 0U; i < n_copy_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_statement_copy(&allocator.base, statement));
  }

  serd_statement_free(&allocator.base, copy);
  serd_statement_free(&allocator.base, statement);
  serd_caret_free(serd_default_allocator(), caret);
  serd_nodes_free(nodes);
}

int
main(void)
{
  test_invalid_new();
  test_copy();
  test_copy_with_caret();
  test_free();
  test_fields();
  test_failed_alloc();

  return 0;
}
