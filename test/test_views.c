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

ZIX_PURE_FUNC int
main(void)
{
  test_token_view();
  test_object_view();
  test_no_token();
  test_no_object();
  return 0;
}
