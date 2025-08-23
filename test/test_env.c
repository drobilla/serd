// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/env.h>
#include <serd/event.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/string_pair_view.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define NS_EG "http://example.org/"

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a env to count the number of allocations
  SerdEnv* const env = serd_env_new(&allocator.base, zix_empty_string());
  assert(env);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_env_new(&allocator.base, zix_empty_string()));
  }

  serd_env_free(env);
}

static void
test_set_base_failed_alloc(void)
{
  static const ZixStringView base_uri =
    ZIX_STATIC_STRING("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env = serd_env_new(&allocator.base, zix_empty_string());

  serd_failing_allocator_reset(&allocator, 0);

  const SerdStatus st = serd_env_set_base_uri(env, base_uri);
  assert(st == SERD_BAD_ALLOC);

  serd_env_free(env);
}

static void
test_set_prefix_existing_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env = serd_env_new(&allocator.base, zix_empty_string());
  SerdStatus           st  = SERD_SUCCESS;

  serd_failing_allocator_reset(&allocator, 3);

  st = serd_env_set_prefix(
    env, zix_string("eg"), zix_string("http://example.com/"));
  assert(st == SERD_SUCCESS);

  st = serd_env_set_prefix(
    env, zix_string("eg"), zix_string("http://example.org/"));
  assert(st == SERD_BAD_ALLOC);

  serd_env_free(env);
}

static void
test_set_prefix_absolute_failed_alloc(void)
{
  static const ZixStringView base_uri =
    ZIX_STATIC_STRING("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env       = serd_env_new(&allocator.base, base_uri);
  SerdStatus           st        = SERD_SUCCESS;
  char                 name[64]  = "eg";
  char                 uri[64]   = "http://example.org/";

  // Successfully set an absolute prefix to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
  assert(st == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_prefix_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_prefix_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);

    snprintf(name, sizeof(name), "eg%zu", i);
    snprintf(uri, sizeof(name), "http://example.org/%zu", i);

    st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
    assert(st == SERD_BAD_ALLOC);
  }

  serd_env_free(env);
}

static void
test_set_prefix_relative_failed_alloc(void)
{
  static const ZixStringView base_uri =
    ZIX_STATIC_STRING("http://example.org/");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdEnv* const       env       = serd_env_new(&allocator.base, base_uri);
  SerdStatus           st        = SERD_SUCCESS;
  char                 name[64]  = "egX";
  char                 uri[64]   = "relativeX";

  // Successfully set an absolute prefix to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
  assert(st == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_prefix_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_prefix_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);

    snprintf(name, sizeof(name), "eg%zu", i);
    snprintf(uri, sizeof(uri), "relative%zu", i);

    st = serd_env_set_prefix(env, zix_string(name), zix_string(uri));
    assert(st == SERD_BAD_ALLOC);
  }

  serd_env_free(env);
}

static void
test_null(void)
{
  // Accessors are tolerant to a NULL env for convenience
  SerdStringPairView pair = {{"", 0}, {"", 0}};
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(NULL)));
  assert(!serd_env_prefix_uri(NULL, zix_string("name")).length);
  assert(serd_env_expand(NULL, zix_empty_string(), &pair) == SERD_BAD_ARG);
  assert(serd_env_qualify(NULL, zix_empty_string(), &pair) == SERD_BAD_ARG);
  assert(serd_env_resolve(NULL, serd_no_token(), &pair) == SERD_BAD_ARG);
}

static SerdStatus
count_prefixes(void* const handle, const SerdEvent* const event)
{
  *(size_t*)handle += event->type == SERD_EVENT_PREFIX;

  return SERD_SUCCESS;
}

static void
test_base_uri(void)
{
  assert(!serd_env_new(NULL, zix_string("rel")));
  assert(!serd_env_base_uri_string(NULL).length);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  // Test that empty/unset base works as expected
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(env)));
  assert(!serd_env_base_uri_string(env).length);
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(env)));
  assert(!serd_env_base_uri_string(env).length);

  // Try setting a relative base with no previous base URI
  assert(serd_env_set_base_uri(env, zix_string("rel")) == SERD_BAD_ARG);

  // Try setting a relative prefix with no base URI
  assert(serd_env_set_prefix(env, zix_string("eg.3"), zix_string("rel")) ==
         SERD_BAD_ARG);

  // Set a valid base URI
  assert(!serd_env_set_base_uri(env, zix_string(NS_EG)));
  assert(expect_string_view(serd_env_base_uri_string(env), NS_EG));

  // Reset the base URI
  assert(!serd_env_set_base_uri(env, zix_empty_string()));
  assert(expect_string_view(serd_env_base_uri_string(env), ""));
  assert(!serd_uri_has_scheme(serd_env_base_uri_view(env)));

  serd_env_free(env);
}

static void
test_set_prefix(void)
{
  static const ZixStringView eg    = ZIX_STATIC_STRING(NS_EG);
  static const ZixStringView name1 = ZIX_STATIC_STRING("eg.1");
  static const ZixStringView name2 = ZIX_STATIC_STRING("eg.2");
  static const ZixStringView rel   = ZIX_STATIC_STRING("rel");
  static const ZixStringView base  = ZIX_STATIC_STRING(NS_EG);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  // Ensure that a prefix isn't initially set
  assert(!serd_env_prefix_uri(env, name1).length);
  assert(serd_env_prefix_uri(env, name1).data);
  assert(serd_env_prefix_uri(env, name1).data[0] == '\0');

  // Set a valid prefix
  assert(!serd_env_set_prefix(env, name1, eg));
  assert(expect_string_view(serd_env_prefix_uri(env, name1), eg.data));

  // Test setting a prefix from a relative URI
  assert(serd_env_set_prefix(env, name2, rel) == SERD_BAD_ARG);
  assert(!serd_env_set_base_uri(env, base));
  assert(!serd_env_set_prefix(env, name2, rel));

  // Test setting a prefix from strings
  assert(
    !serd_env_set_prefix(env, zix_string("eg.3"), zix_string(NS_EG "three")));

  // Count prefixes
  size_t         count = 0;
  const SerdSink sink  = {&count, count_prefixes};
  serd_env_write_prefixes(env, &sink);
  assert(count == 3);

  serd_env_free(env);
}

static SerdTokenView
uri_token(const ZixStringView string)
{
  return serd_token_view(SERD_URI, string);
}

static void
test_resolve(void)
{
  static const ZixStringView base = ZIX_STATIC_STRING(NS_EG "b/");

  SerdEnv* const env = serd_env_new(NULL, base);
  assert(env);

  SerdStringPairView pair = {{"", 0}, {"", 0}};

  assert(serd_env_resolve(NULL, uri_token(base), &pair) == SERD_BAD_ARG);

  assert(serd_env_resolve(env,
                          serd_token_view(SERD_BLANK, zix_string("b0")),
                          &pair) == SERD_BAD_ARG);

  assert(!serd_env_resolve(env, uri_token(zix_empty_string()), &pair));
  assert(serd_string_pair_view_equals_string(pair, base));

  assert(!serd_env_resolve(env, uri_token(base), &pair));
  assert(serd_string_pair_view_equals_string(pair, base));

  assert(!serd_env_resolve(env, uri_token(zix_string("r")), &pair));
  assert(serd_string_pair_view_equals_string(pair, zix_string(NS_EG "b/r")));

  assert(!serd_env_resolve(env, uri_token(zix_string("r/s")), &pair));
  assert(serd_string_pair_view_equals_string(pair, zix_string(NS_EG "b/r/s")));

  serd_env_set_base_uri(env, zix_empty_string());
  assert(serd_env_resolve(env, uri_token(zix_string("r")), &pair) ==
         SERD_BAD_URI);

  serd_env_free(env);
}

static void
test_resolve_pathless_base(void)
{
#define PATHLESS "http://example.org"

  static const ZixStringView base  = ZIX_STATIC_STRING(PATHLESS);
  static const SerdTokenView empty = {SERD_URI, ZIX_STATIC_STRING("")};
  static const SerdTokenView rel   = {SERD_URI, ZIX_STATIC_STRING("rel")};

  SerdEnv* const env = serd_env_new(NULL, base);
  assert(env);

  SerdStringPairView pair = {{"", 0}, {"", 0}};
  assert(!serd_env_resolve(env, empty, &pair));
  assert(serd_string_pair_view_equals_string(pair, zix_string(PATHLESS "/")));

  assert(!serd_env_resolve(env, rel, &pair));
  assert(serd_string_pair_view_equals_string(pair, zix_string(NS_EG "rel")));

  serd_env_free(env);

#undef PATHLESS
}

static void
test_expand_curie(void)
{
  static const ZixStringView name  = ZIX_STATIC_STRING("eg.1");
  static const ZixStringView eg    = ZIX_STATIC_STRING(NS_EG);
  static const ZixStringView curie = ZIX_STATIC_STRING("eg.1:foo");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, name, eg));

  SerdStringPairView pair = {{"", 0}, {"", 0}};
  assert(!serd_env_expand(env, curie, &pair));
  assert(pair.prefix.data);
  assert(expect_string_view(pair.prefix, NS_EG));
  assert(pair.suffix.data);
  assert(expect_string_view(pair.suffix, "foo"));

  serd_env_free(env);
}

static void
test_expand_bad_curie(void)
{
  static const ZixStringView prefixed   = ZIX_STATIC_STRING("eg:foo");
  static const ZixStringView unprefixed = ZIX_STATIC_STRING("bar");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  SerdStringPairView pair = {{"", 0}, {"", 0}};
  assert(serd_env_expand(env, prefixed, &pair) == SERD_BAD_CURIE);
  assert(!pair.prefix.length);
  assert(!pair.suffix.length);

  assert(serd_env_expand(env, unprefixed, &pair) == SERD_BAD_CURIE);
  assert(!pair.prefix.length);
  assert(!pair.suffix.length);

  serd_env_free(env);
}

static void
test_qualify(void)
{
  static const ZixStringView eg = ZIX_STATIC_STRING(NS_EG);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  assert(!serd_env_set_prefix(env, zix_string("eg"), eg));

  SerdStringPairView pair = {zix_empty_string(), zix_empty_string()};
  assert(!serd_env_qualify(env, zix_string(NS_EG "foo"), &pair));
  assert(pair.prefix.length == 2);
  assert(expect_string_view(pair.prefix, "eg"));
  assert(pair.suffix.length == 3);
  assert(expect_string_view(pair.suffix, "foo"));

  assert(serd_env_qualify(env, zix_string("http://drobilla.net/bar"), &pair) ==
         SERD_FAILURE);

  serd_env_free(env);
}

static void
test_sink(void)
{
  static ZixStringView base = ZIX_STATIC_STRING(NS_EG);
  static ZixStringView name = ZIX_STATIC_STRING("eg");
  static ZixStringView uri  = ZIX_STATIC_STRING(NS_EG "uri");

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());

  const SerdSink* const sink = serd_env_sink(env);

  assert(!serd_sink_event(
    sink,
    serd_statement_event(
      0U,
      serd_triple_view(uri_token(uri),
                       uri_token(uri),
                       serd_token_object_view(uri_token(uri))))));

  assert(!serd_sink_event(sink, serd_base_event(base)));
  assert(expect_string_view(serd_env_base_uri_string(env), NS_EG));

  assert(!serd_sink_event(sink, serd_prefix_event(name, uri)));

  const ZixStringView eg_prefix = serd_env_prefix_uri(env, zix_string("eg"));
  assert(zix_string_view_equals(eg_prefix, uri));

  assert(expect_string_view(serd_env_base_uri_string(env), NS_EG));

  serd_env_free(env);
}

static SerdStatus
on_describe_event(void* const handle, const SerdEvent* const event)
{
  (void)handle;
  (void)event;
  return SERD_BAD_LITERAL; // A status not used internally by the reader
}

static void
test_describe(void)
{
  static ZixStringView name = ZIX_STATIC_STRING("eg");
  static ZixStringView uri  = ZIX_STATIC_STRING(NS_EG "uri");

  SerdEnv* const        env      = serd_env_new(NULL, uri);
  const SerdSink* const env_sink = serd_env_sink(env);

  assert(!serd_sink_event(env_sink, serd_prefix_event(name, uri)));

  const SerdSink   describe_sink = {NULL, on_describe_event};
  const SerdStatus st            = serd_env_write_prefixes(env, &describe_sink);
  assert(st == SERD_BAD_LITERAL);

  serd_env_free(env);
}

static bool
tokens_equal(const SerdEnv* const env,
             const SerdNodeType   lhs_type,
             const char* const    lhs_string,
             const SerdNodeType   rhs_type,
             const char* const    rhs_string)
{
  return serd_env_tokens_equal(
    env,
    serd_token_view(lhs_type, zix_string(lhs_string)),
    serd_token_view(rhs_type, zix_string(rhs_string)));
}

static void
test_tokens_equal(void)
{
  SerdEnv* const env = serd_env_new(NULL, zix_string(NS_EG));

  assert(!serd_env_set_prefix(env, zix_string("eg"), zix_string(NS_EG "p/")));

  assert(tokens_equal(env, SERD_BLANK, "b0", SERD_BLANK, "b0"));
  assert(tokens_equal(env, SERD_BLANK, "b1", SERD_BLANK, "b1"));
  assert(!tokens_equal(env, SERD_URI, "x", SERD_LITERAL, "x"));
  assert(!tokens_equal(env, SERD_LITERAL, "x", SERD_URI, "x"));
  assert(!tokens_equal(env, SERD_CURIE, "eg:x", SERD_LITERAL, "eg:x"));
  assert(!tokens_equal(env, SERD_LITERAL, "eg:x", SERD_CURIE, "eg:x"));
  assert(tokens_equal(env, SERD_URI, NS_EG "x", SERD_URI, NS_EG "x"));
  assert(tokens_equal(env, SERD_CURIE, "eg:x", SERD_URI, NS_EG "p/x"));
  assert(tokens_equal(env, SERD_URI, NS_EG "p/x", SERD_CURIE, "eg:x"));
  assert(!tokens_equal(env, SERD_URI, NS_EG "p/x", SERD_CURIE, "unset:x"));
  assert(!tokens_equal(env, SERD_CURIE, "unset:x", SERD_URI, NS_EG "p/x"));
  assert(!tokens_equal(env, SERD_CURIE, "eg:x", SERD_URI, NS_EG "p/y"));
  assert(!tokens_equal(env, SERD_URI, NS_EG "p/x", SERD_CURIE, "eg:y"));
  assert(tokens_equal(env, SERD_CURIE, "eg:x", SERD_CURIE, "eg:x"));
  assert(!tokens_equal(env, SERD_CURIE, "eg:x", SERD_CURIE, "eg:y"));
  assert(tokens_equal(env, SERD_URI, "r", SERD_URI, NS_EG "r"));
  assert(!tokens_equal(env, SERD_URI, "r", SERD_URI, NS_EG "s"));
  assert(!tokens_equal(env, SERD_URI, "s", SERD_URI, NS_EG "r"));

  serd_env_free(env);
}

static SerdObjectView
literal_view(const char* const   string,
             const SerdNodeFlags flags,
             const SerdNodeType  meta_type,
             const char* const   meta_string)
{
  return serd_object_view(SERD_LITERAL,
                          zix_string(string),
                          flags,
                          serd_token_view(meta_type, zix_string(meta_string)));
}

static void
test_objects_equal(void)
{
  SerdEnv* const env = serd_env_new(NULL, zix_string(NS_EG));

  assert(!serd_env_set_prefix(env, zix_string("eg"), zix_string(NS_EG "p/")));

  assert(!serd_env_objects_equal(
    env,
    serd_token_object_view(serd_token_view(SERD_BLANK, zix_string("b0"))),
    serd_token_object_view(serd_token_view(SERD_LITERAL, zix_string("b0")))));

  assert(!serd_env_objects_equal(
    env,
    serd_token_object_view(serd_token_view(SERD_LITERAL, zix_string("b0"))),
    serd_token_object_view(serd_token_view(SERD_BLANK, zix_string("b0")))));

  assert(!serd_env_objects_equal(
    env,
    literal_view("hello", SERD_HAS_LANGUAGE, SERD_LITERAL, "en"),
    literal_view("hello", SERD_HAS_DATATYPE, SERD_URI, NS_EG "String")));

  assert(serd_env_objects_equal(
    env,
    literal_view("hello", SERD_HAS_LANGUAGE, SERD_LITERAL, "en"),
    literal_view("hello", SERD_HAS_LANGUAGE, SERD_LITERAL, "en")));

  assert(serd_env_objects_equal(
    env,
    literal_view("hello", SERD_HAS_LANGUAGE | SERD_IS_LONG, SERD_LITERAL, "en"),
    literal_view("hello", SERD_HAS_LANGUAGE, SERD_LITERAL, "en")));

  assert(!serd_env_objects_equal(
    env,
    literal_view("hello", SERD_HAS_LANGUAGE, SERD_LITERAL, "en"),
    literal_view("hullo", SERD_HAS_LANGUAGE, SERD_LITERAL, "en")));

  assert(!serd_env_objects_equal(
    env,
    literal_view("hello", SERD_HAS_LANGUAGE, SERD_LITERAL, "en"),
    literal_view("hello", SERD_HAS_LANGUAGE, SERD_LITERAL, "de")));

  assert(serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "Num"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "Num")));

  assert(!serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "Num"),
    literal_view("2", SERD_HAS_DATATYPE, SERD_URI, NS_EG "Num")));

  assert(!serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "Num"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "Int")));

  assert(serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "p/Num"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Num")));

  assert(serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Num"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "p/Num")));

  assert(serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Num"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Num")));

  assert(!serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Num"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Int")));

  assert(!serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "p/Num"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Int")));

  assert(!serd_env_objects_equal(
    env,
    literal_view("1", SERD_HAS_DATATYPE, SERD_CURIE, "eg:Int"),
    literal_view("1", SERD_HAS_DATATYPE, SERD_URI, NS_EG "p/Num")));

  serd_env_free(env);
}

int
main(void)
{
  test_new_failed_alloc();
  test_set_base_failed_alloc();
  test_set_prefix_existing_failed_alloc();
  test_set_prefix_absolute_failed_alloc();
  test_set_prefix_relative_failed_alloc();
  test_null();
  test_base_uri();
  test_set_prefix();
  test_resolve();
  test_resolve_pathless_base();
  test_expand_curie();
  test_expand_bad_curie();
  test_qualify();
  test_sink();
  test_describe();
  test_tokens_equal();
  test_objects_equal();
  return 0;
}
