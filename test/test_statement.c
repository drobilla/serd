// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>

#define NS_EG "http://example.org/"

static void
test_copy(void)
{
  assert(!serd_statement_copy(NULL));

  SerdNode* const f = serd_new_string(serd_string("file"));
  SerdNode* const s = serd_new_uri(serd_string(NS_EG "s"));
  SerdNode* const p = serd_new_uri(serd_string(NS_EG "p"));
  SerdNode* const o = serd_new_uri(serd_string(NS_EG "o"));
  SerdNode* const g = serd_new_uri(serd_string(NS_EG "g"));

  SerdCaret* const     caret     = serd_caret_new(f, 1, 1);
  SerdStatement* const statement = serd_statement_new(s, p, o, g, caret);
  SerdStatement* const copy      = serd_statement_copy(statement);

  assert(serd_statement_equals(copy, statement));
  assert(serd_caret_equals(serd_statement_caret(copy), caret));

  serd_statement_free(copy);
  serd_caret_free(caret);
  serd_statement_free(statement);
  serd_node_free(g);
  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_node_free(f);
}

static void
test_free(void)
{
  serd_statement_free(NULL);
}

static void
test_fields(void)
{
  SerdNode* const f = serd_new_string(serd_string("file"));
  SerdNode* const s = serd_new_uri(serd_string(NS_EG "s"));
  SerdNode* const p = serd_new_uri(serd_string(NS_EG "p"));
  SerdNode* const o = serd_new_uri(serd_string(NS_EG "o"));
  SerdNode* const g = serd_new_uri(serd_string(NS_EG "g"));

  SerdCaret* const     caret     = serd_caret_new(f, 1, 1);
  SerdStatement* const statement = serd_statement_new(s, p, o, g, caret);

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

  SerdStatement* const diff_s = serd_statement_new(o, p, o, g, caret);
  assert(!serd_statement_equals(statement, diff_s));
  serd_statement_free(diff_s);

  SerdStatement* const diff_p = serd_statement_new(s, o, o, g, caret);
  assert(!serd_statement_equals(statement, diff_p));
  serd_statement_free(diff_p);

  SerdStatement* const diff_o = serd_statement_new(s, p, s, g, caret);
  assert(!serd_statement_equals(statement, diff_o));
  serd_statement_free(diff_o);

  SerdStatement* const diff_g = serd_statement_new(s, p, o, s, caret);
  assert(!serd_statement_equals(statement, diff_g));
  serd_statement_free(diff_g);

  serd_statement_free(statement);
  serd_caret_free(caret);
  serd_node_free(g);
  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_node_free(f);
}

int
main(void)
{
  test_copy();
  test_free();
  test_fields();

  return 0;
}
