// Copyright 2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/attributes.h"
#include "serd/syntax.h"

#include <assert.h>

static void
test_syntax_by_name(void)
{
  assert(serd_syntax_by_name("unknown") == (SerdSyntax)0);
  assert(serd_syntax_by_name("") == (SerdSyntax)0);

  assert(serd_syntax_by_name("Turtle") == SERD_TURTLE);
  assert(serd_syntax_by_name("NTriples") == SERD_NTRIPLES);
  assert(serd_syntax_by_name("NQuads") == SERD_NQUADS);
  assert(serd_syntax_by_name("TriG") == SERD_TRIG);

  assert(serd_syntax_by_name("turtle") == SERD_TURTLE);
  assert(serd_syntax_by_name("ntriples") == SERD_NTRIPLES);
  assert(serd_syntax_by_name("nquads") == SERD_NQUADS);
  assert(serd_syntax_by_name("trig") == SERD_TRIG);
}

static void
test_guess_syntax(void)
{
  assert(serd_guess_syntax("file.txt") == (SerdSyntax)0);
  assert(serd_guess_syntax("") == (SerdSyntax)0);
  assert(serd_guess_syntax("nodot.") == (SerdSyntax)0);
  assert(serd_guess_syntax("noext.") == (SerdSyntax)0);
  assert(serd_guess_syntax(".hidden") == (SerdSyntax)0);

  assert(serd_guess_syntax("file.ttl") == SERD_TURTLE);
  assert(serd_guess_syntax("file.nt") == SERD_NTRIPLES);
  assert(serd_guess_syntax("file.nq") == SERD_NQUADS);
  assert(serd_guess_syntax("file.trig") == SERD_TRIG);
}

static void
test_syntax_has_graphs(void)
{
  assert(!serd_syntax_has_graphs((SerdSyntax)0));
  assert(!serd_syntax_has_graphs(SERD_TURTLE));
  assert(!serd_syntax_has_graphs(SERD_NTRIPLES));
  assert(serd_syntax_has_graphs(SERD_NQUADS));
  assert(serd_syntax_has_graphs(SERD_TRIG));
}

SERD_PURE_FUNC int
main(void)
{
  test_syntax_by_name();
  test_guess_syntax();
  test_syntax_has_graphs();
  return 0;
}
