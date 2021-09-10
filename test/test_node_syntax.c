// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static void
test_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdNode* const node = serd_new_string(&allocator.base, serd_string("node"));

  // Successfully convert a node to count the number of allocations

  const size_t n_setup_allocs = allocator.n_allocations;

  char* const str =
    serd_node_to_syntax(&allocator.base, node, SERD_TURTLE, NULL);

  SerdNode* const copy =
    serd_node_from_syntax(&allocator.base, str, SERD_TURTLE, NULL);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_setup_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;

    char* const s =
      serd_node_to_syntax(&allocator.base, node, SERD_TURTLE, NULL);

    SerdNode* const c =
      serd_node_from_syntax(&allocator.base, str, SERD_TURTLE, NULL);

    assert(!s || !c);

    serd_free(&allocator.base, s);
    serd_node_free(&allocator.base, c);
  }

  serd_node_free(&allocator.base, copy);
  serd_free(&allocator.base, str);
  serd_node_free(&allocator.base, node);
}

static bool
check(SerdWorld* const  world,
      const SerdSyntax  syntax,
      SerdNode* const   node,
      const char* const expected)
{
  SerdEnv* const env =
    serd_env_new(world, serd_string("http://example.org/base/"));

  char* const     str  = serd_node_to_syntax(NULL, node, syntax, env);
  SerdNode* const copy = serd_node_from_syntax(NULL, str, syntax, env);

  const bool success = !strcmp(str, expected) && serd_node_equals(copy, node);

  serd_node_free(serd_world_allocator(world), copy);
  serd_free(serd_world_allocator(world), str);
  serd_node_free(NULL, node);
  serd_env_free(env);
  return success;
}

static void
test_common(SerdWorld* const world, const SerdSyntax syntax)
{
  static const int data[] = {4, 2};

  const SerdStringView datatype = serd_string("http://example.org/Datatype");
  const SerdStringView num_type = serd_string("http://example.org/Decimal");

  assert(check(
    world, syntax, serd_new_string(NULL, serd_string("node")), "\"node\""));

  assert(
    check(world,
          syntax,
          serd_new_literal(
            NULL, serd_string("hallo"), SERD_HAS_LANGUAGE, serd_string("de")),
          "\"hallo\"@de"));

  assert(
    check(world,
          syntax,
          serd_new_literal(NULL, serd_string("X"), SERD_HAS_DATATYPE, datatype),
          "\"X\"^^<http://example.org/Datatype>"));

  assert(check(world,
               syntax,
               serd_new_token(NULL, SERD_BLANK, serd_string("blank")),
               "_:blank"));

  assert(check(world,
               syntax,
               serd_new_token(NULL, SERD_BLANK, serd_string("b0")),
               "_:b0"));

  assert(check(world,
               syntax,
               serd_new_token(NULL, SERD_BLANK, serd_string("named1")),
               "_:named1"));

  assert(check(world,
               syntax,
               serd_new_uri(NULL, serd_string("http://example.org/")),
               "<http://example.org/>"));

  assert(check(world,
               syntax,
               serd_new_double(NULL, 1.25),
               "\"1.25E0\"^^<http://www.w3.org/2001/XMLSchema#double>"));

  assert(check(world,
               syntax,
               serd_new_float(NULL, 1.25),
               "\"1.25E0\"^^<http://www.w3.org/2001/XMLSchema#float>"));

  assert(check(world,
               syntax,
               serd_new_integer(NULL, 1234, num_type),
               "\"1234\"^^<http://example.org/Decimal>"));

  assert(
    check(world,
          syntax,
          serd_new_base64(NULL, data, sizeof(data), serd_empty_string()),
          "\"BAAAAAIAAAA=\"^^<http://www.w3.org/2001/XMLSchema#base64Binary>"));
}

static void
test_ntriples(void)
{
  SerdWorld* const world = serd_world_new(NULL);

  test_common(world, SERD_NTRIPLES);

  {
    // No relative URIs in NTriples, so converting one fails without an env
    SerdNode* const rel = serd_new_uri(NULL, serd_string("rel/uri"));
    assert(!serd_node_to_syntax(NULL, rel, SERD_NTRIPLES, NULL));
    assert(!serd_node_from_syntax(NULL, "<rel/uri>", SERD_NTRIPLES, NULL));

    // If a relative URI can be expanded then all's well
    SerdEnv* const env =
      serd_env_new(world, serd_string("http://example.org/base/"));
    char* const str = serd_node_to_syntax(NULL, rel, SERD_NTRIPLES, env);
    assert(!strcmp(str, "<http://example.org/base/rel/uri>"));

    SerdNode* const copy = serd_node_from_syntax(NULL, str, SERD_NTRIPLES, env);

    assert(!strcmp(serd_node_string(copy), "http://example.org/base/rel/uri"));

    serd_node_free(serd_world_allocator(world), copy);
    serd_env_free(env);
    serd_free(serd_world_allocator(world), str);
    serd_node_free(NULL, rel);
  }

  assert(check(world,
               SERD_NTRIPLES,
               serd_new_decimal(NULL, 1.25),
               "\"1.25\"^^<http://www.w3.org/2001/XMLSchema#decimal>"));

  assert(check(world,
               SERD_NTRIPLES,
               serd_new_integer(NULL, 1234, serd_empty_string()),
               "\"1234\"^^<http://www.w3.org/2001/XMLSchema#integer>"));

  assert(check(world,
               SERD_NTRIPLES,
               serd_new_boolean(NULL, true),
               "\"true\"^^<http://www.w3.org/2001/XMLSchema#boolean>"));

  assert(check(world,
               SERD_NTRIPLES,
               serd_new_boolean(NULL, false),
               "\"false\"^^<http://www.w3.org/2001/XMLSchema#boolean>"));

  serd_world_free(world);
}

static void
test_turtle(void)
{
  const SerdStringView xsd_integer =
    serd_string("http://www.w3.org/2001/XMLSchema#integer");

  SerdWorld* const world = serd_world_new(NULL);

  test_common(world, SERD_TURTLE);

  check(world,
        SERD_TURTLE,
        serd_new_uri(NULL, serd_string("rel/uri")),
        "<rel/uri>");

  assert(check(world, SERD_TURTLE, serd_new_decimal(NULL, 1.25), "1.25"));

  assert(check(world,
               SERD_TURTLE,
               serd_new_integer(NULL, 1234, serd_empty_string()),
               "1234"));

  assert(check(
    world, SERD_TURTLE, serd_new_integer(NULL, 1234, xsd_integer), "1234"));

  assert(check(world, SERD_TURTLE, serd_new_boolean(NULL, true), "true"));
  assert(check(world, SERD_TURTLE, serd_new_boolean(NULL, false), "false"));

  serd_world_free(world);
}

int
main(void)
{
  test_failed_alloc();
  test_ntriples();
  test_turtle();

  return 0;
}
