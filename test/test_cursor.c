// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/cursor.h>
#include <serd/model.h>
#include <serd/node_args.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/world.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stddef.h>

#define NS_EG "http://example.org/"

static void
test_copy(void)
{
  assert(!serd_cursor_copy(NULL, NULL));

  SerdWorld* const  world = serd_world_new(NULL);
  SerdModel* const  model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  SerdCursor* const begin = serd_model_begin(NULL, model);
  SerdCursor* const copy  = serd_cursor_copy(NULL, begin);

  assert(serd_cursor_equals(copy, begin));

  serd_cursor_free(NULL, copy);
  serd_cursor_free(NULL, begin);
  serd_model_free(model);
  serd_world_free(world);
}

static void
test_comparison(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdModel* const model = serd_model_new(world, SERD_ORDER_SPO, 0U);

  serd_model_add_index(model, SERD_ORDER_OPS);

  const SerdNodeArgs a = serd_a_uri(zix_string(NS_EG "a"));
  const SerdNodeArgs b = serd_a_uri(zix_string(NS_EG "b"));
  const SerdNodeArgs c = serd_a_uri(zix_string(NS_EG "c"));

  // Add a single statement
  assert(!serd_model_add(model, a, b, c, serd_a_null()));

  // Make cursors that point to the statement but via different patterns
  SerdCursor* const c1 = serd_model_find(
    NULL, model, a, serd_a_null(), serd_a_null(), serd_a_null());
  SerdCursor* const c2 =
    serd_model_find(NULL, model, a, b, serd_a_null(), serd_a_null());
  SerdCursor* const c3 =
    serd_model_find(NULL, model, serd_a_null(), b, c, serd_a_null());

  // Ensure that they refer to the same statement but are not equal
  assert(c1);
  assert(c2);
  assert(c3);
  assert(serd_statement_view_equals(serd_cursor_get(c1), serd_cursor_get(c2)));
  assert(serd_statement_view_equals(serd_cursor_get(c2), serd_cursor_get(c3)));
  assert(!serd_cursor_equals(c1, c2));
  assert(!serd_cursor_equals(c2, c3));
  assert(!serd_cursor_equals(c1, c3));

  // Check that none are equal to begin (which has a different mode) or end
  SerdCursor* const begin = serd_model_begin(NULL, model);
  assert(!serd_cursor_equals(c1, begin));
  assert(!serd_cursor_equals(c2, begin));
  assert(!serd_cursor_equals(c3, begin));
  assert(!serd_cursor_equals(c1, serd_model_end(model)));
  assert(!serd_cursor_equals(c2, serd_model_end(model)));
  assert(!serd_cursor_equals(c3, serd_model_end(model)));
  serd_cursor_free(NULL, begin);

  // Check that a cursor that points to it via the same pattern is equal
  SerdCursor* const c4 =
    serd_model_find(NULL, model, a, b, serd_a_null(), serd_a_null());
  assert(c4);
  assert(serd_statement_view_equals(serd_cursor_get(c4), serd_cursor_get(c1)));
  assert(serd_cursor_equals(c4, c2));
  assert(!serd_cursor_equals(c4, c3));
  serd_cursor_free(NULL, c4);

  // Advance everything to the end
  assert(serd_cursor_advance(c1) == SERD_FAILURE);
  assert(serd_cursor_advance(c2) == SERD_FAILURE);
  assert(serd_cursor_advance(c3) == SERD_FAILURE);

  // Check that they are now equal, and equal to the model's end
  assert(serd_cursor_equals(c1, c2));
  assert(serd_cursor_equals(c1, serd_model_end(model)));
  assert(serd_cursor_equals(c2, serd_model_end(model)));

  serd_cursor_free(NULL, c3);
  serd_cursor_free(NULL, c2);
  serd_cursor_free(NULL, c1);
  serd_model_free(model);
  serd_world_free(world);
}

int
main(void)
{
  assert(serd_cursor_advance(NULL) == SERD_FAILURE);

  test_copy();
  test_comparison();

  return 0;
}
