// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool
test(const SerdSyntax syntax, SerdNode* const node, const char* const expected)
{
  SerdEnv* const  env  = serd_env_new(serd_string("http://example.org/base/"));
  char* const     str  = serd_node_to_syntax(node, syntax, env);
  SerdNode* const copy = serd_node_from_syntax(str, syntax, env);

  const bool success = !strcmp(str, expected) && serd_node_equals(copy, node);

  serd_node_free(copy);
  serd_free(str);
  serd_node_free(node);
  serd_env_free(env);
  return success;
}

static void
test_common(const SerdSyntax syntax)
{
  static const int data[] = {4, 2};

  const SerdStringView datatype = serd_string("http://example.org/Datatype");

  SerdNode* const num_type =
    serd_new_uri(serd_string("http://example.org/Decimal"));

  assert(test(syntax, serd_new_string(serd_string("node")), "\"node\""));

  assert(test(syntax,
              serd_new_literal(
                serd_string("hallo"), SERD_HAS_LANGUAGE, serd_string("de")),
              "\"hallo\"@de"));

  assert(test(syntax,
              serd_new_literal(serd_string("X"), SERD_HAS_DATATYPE, datatype),
              "\"X\"^^<http://example.org/Datatype>"));

  assert(test(syntax, serd_new_blank(serd_string("blank")), "_:blank"));
  assert(test(syntax, serd_new_blank(serd_string("b0")), "_:b0"));

  assert(test(syntax,
              serd_new_uri(serd_string("http://example.org/")),
              "<http://example.org/>"));

  assert(test(syntax,
              serd_new_decimal(1.25, num_type),
              "\"1.25\"^^<http://example.org/Decimal>"));

  assert(test(syntax,
              serd_new_double(1.25),
              "\"1.25E0\"^^<http://www.w3.org/2001/XMLSchema#double>"));

  assert(test(syntax,
              serd_new_float(1.25),
              "\"1.25E0\"^^<http://www.w3.org/2001/XMLSchema#float>"));

  assert(test(syntax,
              serd_new_integer(1234, num_type),
              "\"1234\"^^<http://example.org/Decimal>"));

  assert(
    test(syntax,
         serd_new_base64(data, sizeof(data), NULL),
         "\"BAAAAAIAAAA=\"^^<http://www.w3.org/2001/XMLSchema#base64Binary>"));

  serd_node_free(num_type);
}

static void
test_ntriples(void)
{
  test_common(SERD_NTRIPLES);

  {
    // No relative URIs in NTriples, so converting one fails without an env
    SerdNode* const rel = serd_new_uri(serd_string("rel/uri"));
    assert(!serd_node_to_syntax(rel, SERD_NTRIPLES, NULL));
    assert(!serd_node_from_syntax("<rel/uri>", SERD_NTRIPLES, NULL));

    // If a relative URI can be expanded then all's well
    SerdEnv* const env = serd_env_new(serd_string("http://example.org/base/"));
    char* const    str = serd_node_to_syntax(rel, SERD_NTRIPLES, env);
    assert(!strcmp(str, "<http://example.org/base/rel/uri>"));

    SerdNode* const copy = serd_node_from_syntax(str, SERD_NTRIPLES, env);
    assert(!strcmp(serd_node_string(copy), "http://example.org/base/rel/uri"));

    serd_node_free(copy);
    serd_env_free(env);
    serd_free(str);
    serd_node_free(rel);
  }

  assert(test(SERD_NTRIPLES,
              serd_new_decimal(1.25, NULL),
              "\"1.25\"^^<http://www.w3.org/2001/XMLSchema#decimal>"));

  assert(test(SERD_NTRIPLES,
              serd_new_integer(1234, NULL),
              "\"1234\"^^<http://www.w3.org/2001/XMLSchema#integer>"));

  assert(test(SERD_NTRIPLES,
              serd_new_boolean(true),
              "\"true\"^^<http://www.w3.org/2001/XMLSchema#boolean>"));

  assert(test(SERD_NTRIPLES,
              serd_new_boolean(false),
              "\"false\"^^<http://www.w3.org/2001/XMLSchema#boolean>"));
}

static void
test_turtle(void)
{
  test_common(SERD_TURTLE);
  test(SERD_TURTLE, serd_new_uri(serd_string("rel/uri")), "<rel/uri>");
  assert(test(SERD_TURTLE, serd_new_decimal(1.25, NULL), "1.25"));
  assert(test(SERD_TURTLE, serd_new_integer(1234, NULL), "1234"));
  assert(test(SERD_TURTLE, serd_new_boolean(true), "true"));
  assert(test(SERD_TURTLE, serd_new_boolean(false), "false"));
}

int
main(void)
{
  test_ntriples();
  test_turtle();

  return 0;
}
