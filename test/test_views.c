// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/statement_view.h>
#include <serd/token_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>

#define NS_EG "http://example.org/"

static void
test_token_view(void)
{
  const SerdTokenView tok = serd_token_view(SERD_LITERAL, zix_string("a\nb"));
  assert(tok.type == SERD_LITERAL);
  assert(zix_string_view_equals(tok.string, zix_string("a\nb")));
}

static void
test_object_view(void)
{
  const SerdObjectView obj =
    serd_object_view(SERD_LITERAL,
                     zix_string("eh?"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en-ca")));

  assert(obj.type == SERD_LITERAL);
  assert(zix_string_view_equals(obj.string, zix_string("eh?")));
  assert(obj.flags == SERD_HAS_LANGUAGE);
  assert(obj.meta.type == SERD_LITERAL);
  assert(zix_string_view_equals(obj.meta.string, zix_string("en-ca")));

  const SerdTokenView tok = serd_object_token_view(obj);
  assert(tok.type == SERD_LITERAL);
  assert(zix_string_view_equals(tok.string, zix_string("eh?")));

  const SerdObjectView obj2 = serd_token_object_view(tok);
  assert(obj2.type == SERD_LITERAL);
  assert(zix_string_view_equals(obj2.string, zix_string("eh?")));
  assert(!obj2.flags);
  assert(!obj2.meta.string.length);
}

static void
test_no_token(void)
{
  const SerdTokenView tok = serd_no_token();
  assert(tok.type != SERD_LITERAL);
  assert(tok.type != SERD_URI);
  assert(tok.type != SERD_CURIE);
  assert(!tok.string.length);
}

static void
test_no_object(void)
{
  const SerdObjectView obj = serd_no_object();
  assert(obj.type != SERD_LITERAL);
  assert(obj.type != SERD_URI);
  assert(obj.type != SERD_CURIE);
  assert(!obj.string.length);
  assert(!obj.flags);
  assert(!obj.meta.type);
  assert(!obj.meta.string.length);
}

static void
test_statement(void)
{
  const SerdTokenView s = serd_token_view(SERD_BLANK, zix_string("b42"));
  const SerdTokenView p = serd_token_view(SERD_URI, zix_string(NS_EG "p"));
  const SerdTokenView g = serd_token_view(SERD_URI, zix_string(NS_EG "g"));

  const SerdObjectView o =
    serd_object_view(SERD_LITERAL, zix_string("o"), 0U, serd_no_token());

  const SerdStatementView triple = serd_triple_view(s, p, o);
  assert(triple.subject.type == s.type);
  assert(zix_string_view_equals(triple.subject.string, s.string));
  assert(triple.predicate.type == p.type);
  assert(zix_string_view_equals(triple.predicate.string, p.string));
  assert(triple.object.type == o.type);
  assert(zix_string_view_equals(triple.object.string, o.string));
  assert(!triple.graph.type);
  assert(!triple.graph.string.length);

  const SerdStatementView quad = serd_quad_view(s, p, o, g);
  assert(quad.subject.type == s.type);
  assert(zix_string_view_equals(quad.subject.string, s.string));
  assert(quad.predicate.type == p.type);
  assert(zix_string_view_equals(quad.predicate.string, p.string));
  assert(quad.object.type == o.type);
  assert(zix_string_view_equals(quad.object.string, o.string));
  assert(quad.graph.type == g.type);
  assert(zix_string_view_equals(quad.graph.string, g.string));
}

static void
test_no_statement(void)
{
  const SerdStatementView s = serd_no_statement();
  assert(!s.subject.type);
  assert(!s.subject.string.length);
  assert(!s.subject.string.data[0]);
  assert(!s.predicate.type);
  assert(!s.predicate.string.length);
  assert(!s.predicate.string.data[0]);
  assert(!s.object.type);
  assert(!s.object.string.length);
  assert(!s.object.string.data[0]);
  assert(!s.object.flags);
  assert(!s.object.meta.type);
  assert(!s.object.meta.string.length);
  assert(!s.object.meta.string.data[0]);
  assert(!s.graph.type);
  assert(!s.graph.string.length);
  assert(!s.graph.string.data[0]);
}

ZIX_PURE_FUNC int
main(void)
{
  test_token_view();
  test_object_view();
  test_no_token();
  test_no_object();
  test_statement();
  test_no_statement();
  return 0;
}
