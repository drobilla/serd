// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/node_type.h>
#include <serd/token_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>

static void
test_token_view_equals(void)
{
  assert(serd_token_view_equals(serd_no_token(), serd_no_token()));

  assert(serd_token_view_equals(
    serd_token_view(SERD_URI, zix_string("http://example.org/")),
    serd_token_view(SERD_URI, zix_string("http://example.org/"))));

  assert(!serd_token_view_equals(
    serd_token_view(SERD_URI, zix_string("http://example.com/")),
    serd_token_view(SERD_URI, zix_string("http://example.org/"))));

  assert(!serd_token_view_equals(
    serd_token_view(SERD_URI, zix_string("http://example.org")),
    serd_token_view(SERD_URI, zix_string("http://example.org/"))));

  assert(!serd_token_view_equals(
    serd_token_view(SERD_LITERAL, zix_string("http://example.org/")),
    serd_token_view(SERD_URI, zix_string("http://example.org/"))));

  assert(!serd_token_view_equals(
    serd_no_token(),
    serd_token_view(SERD_URI, zix_string("http://example.org/"))));

  assert(!serd_token_view_equals(
    serd_token_view(SERD_URI, zix_string("http://example.org/")),
    serd_no_token()));
}

ZIX_PURE_FUNC int
main(void)
{
  test_token_view_equals();
  return 0;
}
