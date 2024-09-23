// Copyright 2024-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/field.h>
#include <serd/node_type.h>
#include <zix/attributes.h>

#include <assert.h>

static void
test_field_supports(void)
{
  assert(!serd_field_supports(SERD_SUBJECT, SERD_LITERAL));
  assert(serd_field_supports(SERD_SUBJECT, SERD_URI));
  assert(serd_field_supports(SERD_SUBJECT, SERD_CURIE));
  assert(serd_field_supports(SERD_SUBJECT, SERD_BLANK));

  assert(!serd_field_supports(SERD_PREDICATE, SERD_LITERAL));
  assert(serd_field_supports(SERD_PREDICATE, SERD_URI));
  assert(serd_field_supports(SERD_PREDICATE, SERD_CURIE));
  assert(!serd_field_supports(SERD_PREDICATE, SERD_BLANK));

  assert(serd_field_supports(SERD_OBJECT, SERD_LITERAL));
  assert(serd_field_supports(SERD_OBJECT, SERD_URI));
  assert(serd_field_supports(SERD_OBJECT, SERD_CURIE));
  assert(serd_field_supports(SERD_OBJECT, SERD_BLANK));

  assert(!serd_field_supports(SERD_GRAPH, SERD_LITERAL));
  assert(serd_field_supports(SERD_GRAPH, SERD_URI));
  assert(serd_field_supports(SERD_GRAPH, SERD_CURIE));
  assert(serd_field_supports(SERD_GRAPH, SERD_BLANK));
}

ZIX_PURE_FUNC int
main(void)
{
  test_field_supports();
  return 0;
}
