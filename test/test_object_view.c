// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/token_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>

static void
test_object_view_equals(void)
{
  assert(serd_object_view_equals(serd_no_object(), serd_no_object()));

  // URIs (similar to token cases)

  assert(serd_object_view_equals(
    serd_object_view(
      SERD_URI, zix_string("http://example.org/"), 0U, serd_no_token()),
    serd_object_view(
      SERD_URI, zix_string("http://example.org/"), 0U, serd_no_token())));

  assert(!serd_object_view_equals(
    serd_object_view(
      SERD_URI, zix_string("http://example.com/"), 0U, serd_no_token()),
    serd_object_view(
      SERD_URI, zix_string("http://example.org/"), 0U, serd_no_token())));

  assert(!serd_object_view_equals(
    serd_object_view(
      SERD_URI, zix_string("http://example.org"), 0U, serd_no_token()),
    serd_object_view(
      SERD_URI, zix_string("http://example.org/"), 0U, serd_no_token())));

  assert(!serd_object_view_equals(
    serd_object_view(
      SERD_LITERAL, zix_string("http://example.org"), 0U, serd_no_token()),
    serd_object_view(
      SERD_URI, zix_string("http://example.org/"), 0U, serd_no_token())));

  assert(!serd_object_view_equals(
    serd_no_object(),
    serd_object_view(
      SERD_URI, zix_string("http://example.org/"), 0U, serd_no_token())));

  assert(!serd_object_view_equals(
    serd_object_view(
      SERD_URI, zix_string("http://example.org/"), 0U, serd_no_token()),
    serd_no_object()));

  // Literals

  assert(serd_object_view_equals(
    serd_object_view(SERD_LITERAL, zix_string("lit"), 0U, serd_no_token()),
    serd_object_view(SERD_LITERAL, zix_string("lit"), 0U, serd_no_token())));

  assert(serd_object_view_equals(
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en"))),
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en")))));

  assert(!serd_object_view_equals(
    serd_object_view(SERD_LITERAL, zix_string("lit"), 0U, serd_no_token()),
    serd_object_view(SERD_LITERAL, zix_string("lat"), 0U, serd_no_token())));

  assert(!serd_object_view_equals(
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en"))),
    serd_object_view(SERD_LITERAL, zix_string("lat"), 0U, serd_no_token())));

  assert(!serd_object_view_equals(
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en"))),
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("de")))));

  assert(!serd_object_view_equals(
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en"))),
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en-ca")))));

  assert(!serd_object_view_equals(
    serd_object_view(SERD_LITERAL,
                     zix_string("lit"),
                     SERD_HAS_LANGUAGE,
                     serd_token_view(SERD_LITERAL, zix_string("en"))),
    serd_object_view(SERD_LITERAL, zix_string("lat"), 0U, serd_no_token())));
}

ZIX_PURE_FUNC int
main(void)
{
  test_object_view_equals();
  return 0;
}
