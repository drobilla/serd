/*
  Copyright 2018 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdio.h>

static const size_t min_stack_size = 64u;
static const size_t max_stack_size = 1024u;

static SerdStatus
test_size(SerdWorld* const  world,
          const char* const str,
          const SerdSyntax  syntax,
          const size_t      stack_size)
{
  SerdSink*         sink = serd_sink_new(NULL, NULL, NULL);
  SerdReader* const reader =
    serd_reader_new(world, syntax, 0u, sink, stack_size);

  assert(reader);

  serd_reader_start_string(reader, str, NULL);
  const SerdStatus st = serd_reader_read_document(reader);
  serd_reader_free(reader);
  serd_sink_free(sink);

  return st;
}

static void
test_all_sizes(SerdWorld* const  world,
               const char* const str,
               const SerdSyntax  syntax)
{
  // Ensure reading with the maximum stack size succeeds
  SerdStatus st = test_size(world, str, syntax, max_stack_size);
  assert(!st);

  // Test with an increasingly smaller stack
  for (size_t size = max_stack_size; size > min_stack_size; --size) {
    if ((st = test_size(world, str, syntax, size))) {
      assert(st == SERD_ERR_OVERFLOW);
    }
  }

  assert(st == SERD_ERR_OVERFLOW);
}

static void
test_ntriples_overflow(void)
{
  static const char* const test_strings[] = {
    "<http://example.org/s> <http://example.org/p> <http://example.org/o> .",
    NULL,
  };

  SerdWorld* const world = serd_world_new();

  for (const char* const* t = test_strings; *t; ++t) {
    test_all_sizes(world, *t, SERD_NTRIPLES);
  }

  serd_world_free(world);
}

static void
test_turtle_overflow(void)
{
  static const char* const test_strings[] = {
    ":s :p :%99 .",
    ":s :p <http://example.org/> .",
    ":s :p <thisisanabsurdlylongurischeme://because/testing/> .",
    ":s :p eg:foo .",
    ":s :p 1234 .",
    ":s :p (1 2 3 4) .",
    ":s :p ((((((((42)))))))) .",
    ":s :p \"literal\" .",
    ":s :p _:blank .",
    ":s :p true .",
    ":s :p \"\"@en .",
    "(((((((((42))))))))) <http://example.org/p> <http://example.org/o> .",
    "@prefix eg: <http://example.org/ns/test> .",
    "@base <http://example.org/base> .",

    "@prefix eg: <http://example.org/> . \neg:s eg:p eg:o .\n",

    "@prefix ug.dot: <http://example.org/> . \nug.dot:s ug.dot:p ug.dot:o .\n",

    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
    "@prefix øøøøøøøøø: <http://example.org/long> . \n"
    "<http://example.org/somewhatlongsubjecttooffsetthepredicate> øøøøøøøøø:p "
    "øøøøøøøøø:o .\n",

    NULL,
  };

  SerdWorld* const world = serd_world_new();

  for (const char* const* t = test_strings; *t; ++t) {
    test_all_sizes(world, *t, SERD_TURTLE);
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
