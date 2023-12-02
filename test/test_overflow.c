// Copyright 2018 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdio.h>

static const size_t min_stack_size = 4U * sizeof(size_t) + 240U;
static const size_t max_stack_size = 2048U;

static SerdStatus
test_size(SerdWorld* const      world,
          const char* const     str,
          const SerdSyntax      syntax,
          const SerdReaderFlags flags,
          const size_t          stack_size)
{
  SerdLimits limits        = serd_world_limits(world);
  limits.reader_stack_size = stack_size;
  serd_world_set_limits(world, limits);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());
  assert(env);

  SerdReader* const reader =
    serd_reader_new(world, syntax, flags, serd_env_sink(env));
  assert(reader);

  SerdNode*       string_name = serd_new_string(NULL, zix_string("string"));
  const char*     position    = str;
  SerdInputStream in          = serd_open_input_string(&position);
  serd_reader_start(reader, &in, string_name, 1);

  const SerdStatus st = serd_reader_read_document(reader);

  serd_close_input(&in);
  serd_node_free(NULL, string_name);
  serd_reader_free(reader);
  serd_env_free(env);

  return st;
}

static void
test_all_sizes(SerdWorld* const      world,
               const char* const     str,
               const SerdSyntax      syntax,
               const SerdReaderFlags flags)
{
  // Ensure reading with the maximum stack size succeeds
  SerdStatus st = test_size(world, str, syntax, flags, max_stack_size);
  assert(!st);

  // Test with an increasingly smaller stack
  for (size_t size = max_stack_size; size > min_stack_size; --size) {
    if ((st = test_size(world, str, syntax, flags, size))) {
      assert(st == SERD_BAD_STACK);
    }
  }

  assert(st == SERD_BAD_STACK);
}

static void
test_ntriples_overflow(void)
{
  static const char* const test_strings[] = {
    "<http://example.org/s> <http://example.org/p> <http://example.org/o> .",
    "<http://example.org/s> <http://example.org/p> \"literal\" .",
    "<http://example.org/s> <http://example.org/p> _:blank .",
    "<http://example.org/s> <http://example.org/p> \"\"@en .",
    "<http://example.org/s> <http://example.org/p> ?var .",
    NULL,
  };

  SerdWorld* const world = serd_world_new(NULL);

  for (const char* const* t = test_strings; *t; ++t) {
    test_all_sizes(world, *t, SERD_NTRIPLES, SERD_READ_VARIABLES);
  }

  serd_world_free(world);
}

static void
test_turtle_overflow(void)
{
  static const char* const test_strings[] = {
    "<http://example.org/s> <http://example.org/p> <http://example.org/> .",
    "<http://example.org/s> <http://example.org/p> "
    "<thisisanabsurdlylongurischeme://because/testing/> .",
    "<http://example.org/s> <http://example.org/p> 0 , .1 , 2.3 , 4E5, 6e07 .",
    "<http://example.org/s> <http://example.org/p> .7e8 , .9E0 , 1.e2 , 3.E4 .",
    "<http://example.org/s> <http://example.org/p> .2E3 , .4e5 , 6.7E8 , 9.",
    "<http://example.org/s> <http://example.org/p> 0.e1, 2.E3.",
    "<http://example.org/s> <http://example.org/p> (+6e-7 8.2E+9 .1 -.2 +.3) .",
    "<http://example.org/s> <http://example.org/p> (((((((42))))))) .",
    "<http://example.org/s> <http://example.org/p> \"literal\" .",
    "<http://example.org/s> <http://example.org/p> \"\"\" \"\"q\"\" \"\"\" .",
    "<http://example.org/s> <http://example.org/p> _:blank .",
    "<http://example.org/s> <http://example.org/p> true .",
    "<http://example.org/s> <http://example.org/p> \"\"@en .",
    "?subject ?predicate ?object .",
    "(((((((((42))))))))) <http://example.org/p> <http://example.org/o> .",
    "@prefix eg: <http://example.org/ns/test> .",
    "@base <http://example.org/base> .",

    "@prefix eg: <http://example.org/> . \neg:s eg:p eg:o .\n",

    "@prefix ug.dot: <http://example.org/> . \nug.dot:s ug.dot:p ug.dot:o .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix øøøøøøøøø: <http://example.org/long> . \n"
    "<http://example.org/somewhatlongsubjecttooffsetthepredicate> øøøøøøøøø:p "
    "øøøøøøøøø:o .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "<http://example.org/subject/with/a/long/path> "
    "<http://example.org/predicate/with/a/long/path> "
    "<http://example.org/object/with/a/long/path> .",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "<http://example.org/s> <http://example.org/p> "
    "\"typed\"^^<http://example.org/Datatype> .",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix eg: <http://example.org/ns/test> .\n"
    "<http://example.org/s> <http://example.org/p> "
    "\"typed\"^^eg:Datatype .",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix eg: <http://example.org/ns/test> .\n"
    "<http://example.org/s> <http://example.org/p> eg:foo .",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix prefix: <http://example.org/testing/curies> .\n"
    "prefix:subject prefix:predicate prefix:object .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix prefix: <http://example.org/testing/curies> .\n"
    "prefix:subjectthatwillcomearoundtobeingfinishedanycharacternow "
    "prefix:predicate prefix:object .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix eg: <http://example.org/> .\n"
    "eg:s eg:p [ eg:p [ eg:p [ eg:p [ eg:p eg:o ] ] ] ] .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix eg: <http://example.org/> .\n"
    "eg:s eg:p ( 1 2 3 ( 4 5 6 ( 7 8 9 ) ) ) .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix eg: <http://example.org/ns/test> .\n"
    "<http://example.org/s> <http://example.org/p> eg:%99 .",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix øøøøøøøøø: <http://example.org/long> .\n"
    "<http://example.org/somewhatlongsubjecttooffsetthepredicate> øøøøøøøøø:p "
    "øøøøøøøøø:o .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@base <http://example.org/ns/test> .\n"
    "<http://example.org/s> <http://example.org/p> <rel> .",

    NULL,
  };

  SerdWorld* const world = serd_world_new(NULL);

  for (const char* const* t = test_strings; *t; ++t) {
    test_all_sizes(world, *t, SERD_TURTLE, SERD_READ_VARIABLES);
  }

  serd_world_free(world);
}

int
main(void)
{
  test_ntriples_overflow();
  test_turtle_overflow();
  return 0;
}
