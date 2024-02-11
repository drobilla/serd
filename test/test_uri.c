// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"

#include <serd/string.h>
#include <serd/uri.h>
#include <zix/allocator.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static void
test_uri_string_has_scheme(void)
{
  assert(!serd_uri_string_has_scheme(NULL));

  assert(!serd_uri_string_has_scheme("relative"));
  assert(!serd_uri_string_has_scheme("http"));
  assert(!serd_uri_string_has_scheme("5nostartdigit"));
  assert(!serd_uri_string_has_scheme("+nostartplus"));
  assert(!serd_uri_string_has_scheme("-nostartminus"));
  assert(!serd_uri_string_has_scheme(".nostartdot"));
  assert(!serd_uri_string_has_scheme(":missing"));
  assert(!serd_uri_string_has_scheme("a/slash/is/not/a/scheme/char"));

  assert(serd_uri_string_has_scheme("http://example.org/"));
  assert(serd_uri_string_has_scheme("https://example.org/"));
  assert(serd_uri_string_has_scheme("allapha:path"));
  assert(serd_uri_string_has_scheme("w1thd1g1t5:path"));
  assert(serd_uri_string_has_scheme("with.dot:path"));
  assert(serd_uri_string_has_scheme("with+plus:path"));
  assert(serd_uri_string_has_scheme("with-minus:path"));
}

static void
test_uri_string_length(void)
{
  assert(serd_uri_string_length(serd_parse_uri(NULL)) == 0);
  assert(serd_uri_string_length(serd_parse_uri("http:")) == 5);
  assert(serd_uri_string_length(serd_parse_uri("file:///")) == 8);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org")) == 18);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org/p")) == 20);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org?q")) == 20);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org#f")) == 20);
  assert(serd_uri_string_length(serd_parse_uri("web+ap://example.org/")) == 21);
  assert(serd_uri_string_length(serd_parse_uri("wasm-js:")) == 8);
  assert(serd_uri_string_length(serd_parse_uri("soap.beep:")) == 10);

  // Needs additional slash
  assert(serd_uri_string_length(serd_resolve_uri(
           serd_parse_uri("p"), serd_parse_uri("http://example.org"))) == 20);

  // Up references
  assert(serd_uri_string_length(serd_relative_uri(
           serd_parse_uri("http://example.org/"),
           serd_parse_uri("http://example.org/sub/dir/"))) == 6);
}

static bool
component_equals(const SerdURIComponent a, const SerdURIComponent b)
{
  return (!a.length && !b.length) ||
         (a.length && b.length && a.data && b.data &&
          !strncmp((const char*)a.data, (const char*)b.data, a.length));
}

static bool
component_matches(const SerdURIComponent actual, const char* const expected)
{
  if (!expected) {
    return !actual.data && !actual.length;
  }

  const SerdURIComponent expected_component = {expected, strlen(expected)};
  return component_equals(expected_component, actual);
}

static void
check_uri_parse(const char* const uri_string,
                const char* const scheme,
                const char* const authority,
                const char* const path,
                const char* const query,
                const char* const fragment)
{
  const SerdURIView uri = serd_parse_uri(uri_string);

  // Check that parsing went as expected
  assert(component_matches(uri.scheme, scheme));
  assert(component_matches(uri.authority, authority));
  assert(component_matches(uri.path, path));
  assert(component_matches(uri.query, query));
  assert(component_matches(uri.fragment, fragment));

  // Check that a string can be created from the parsed URI with the same string
  SerdString string = serd_uri_to_string(NULL, uri);
  assert(expect_string_view(serd_string_view(string), uri_string));
  zix_free(NULL, string.data);
}

static void
test_parse_uri(void)
{
  check_uri_parse("http:", "http", NULL, NULL, NULL, NULL);
  check_uri_parse("http://", "http", "", NULL, NULL, NULL);
  check_uri_parse("ftp://example.org", "ftp", "example.org", NULL, NULL, NULL);
  check_uri_parse("example:/p", "example", NULL, "/p", NULL, NULL);
  check_uri_parse("example:?q#f", "example", NULL, NULL, "q", "#f");
  check_uri_parse("example:?q", "example", NULL, NULL, "q", NULL);
  check_uri_parse("p?q", NULL, NULL, "p", "q", NULL);
  check_uri_parse("p?q#f", NULL, NULL, "p", "q", "#f");
  check_uri_parse("p#f", NULL, NULL, "p", NULL, "#f");
  check_uri_parse("ftp://example.org/path?query#fragment",
                  "ftp",
                  "example.org",
                  "/path",
                  "query",
                  "#fragment");
  check_uri_parse("//example.org/path?query#fragment",
                  NULL,
                  "example.org",
                  "/path",
                  "query",
                  "#fragment");
  check_uri_parse("example.org/path?query#fragment",
                  NULL,
                  NULL,
                  "example.org/path",
                  "query",
                  "#fragment");
  check_uri_parse("?query#fragment", NULL, NULL, NULL, "query", "#fragment");
  check_uri_parse("#fragment", NULL, NULL, NULL, NULL, "#fragment");
  check_uri_parse("", NULL, NULL, NULL, NULL, NULL);

  // Check that a string can be created from the parsed URI with the same string
  const SerdURIView noslash_uri = {{"http", 4U},
                                   {"example.org", 11U},
                                   {"", 0U},
                                   {"noslash", 7U},
                                   {"q", 1U},
                                   {"#f", 2U}};

  SerdString noslash_string = serd_uri_to_string(NULL, noslash_uri);
  assert(expect_string(noslash_string.data, "http://example.org/noslash?q#f"));
  zix_free(NULL, noslash_string.data);
}

static void
check_round_trip(const char* const uri_string)
{
  const SerdURIView uri    = serd_parse_uri(uri_string);
  const SerdString  string = serd_uri_to_string(NULL, uri);

  assert(expect_string_view(serd_string_view(string), uri_string));

  zix_free(NULL, string.data);
}

static void
test_uri_to_string(void)
{
  check_round_trip("rel/path");
  check_round_trip("file:///dir/path");
  check_round_trip("http://example.org/p");
  check_round_trip("http://example.org/p/");
  check_round_trip("http://example.org/p/n");
  check_round_trip("http://example.org/p/n#f");
  check_round_trip("http://example.org/p/n?q#f");
}

static void
check_is_within(const char* const uri_string,
                const char* const base_uri_string,
                const bool        expected)
{
  const SerdURIView uri      = serd_parse_uri(uri_string);
  const SerdURIView base_uri = serd_parse_uri(base_uri_string);

  assert(serd_uri_is_within(uri, base_uri) == expected);
}

static void
test_is_within(void)
{
  static const char* const base = "http://example.org/base/";

  check_is_within("http://example.org/base/", base, true);
  check_is_within("http://example.org/base/kid?q", base, true);
  check_is_within("http://example.org/base/kid", base, true);
  check_is_within("http://example.org/base/kid#f", base, true);
  check_is_within("http://example.org/base/kid?q#f", base, true);
  check_is_within("http://example.org/base/kid/grandkid", base, true);

  check_is_within("http://example.org/base", base, false);
  check_is_within("http://example.org/based", base, false);
  check_is_within("http://example.org/bose", base, false);
  check_is_within("http://example.org/", base, false);
  check_is_within("http://other.org/base", base, false);
  check_is_within("ftp://other.org/base", base, false);
  check_is_within("base", base, false);

  check_is_within("http://example.org/", "rel", false);
}

static void
check_relative(const char* const uri_string,
               const char* const base_string,
               const char* const expected_string)
{
  assert(uri_string);
  assert(base_string);
  assert(expected_string);

  SerdURIView uri  = serd_parse_uri(uri_string);
  SerdURIView base = serd_parse_uri(base_string);

  SerdString result_string =
    serd_uri_to_string(NULL, serd_relative_uri(uri, base));

  assert(expect_string_view(serd_string_view(result_string), expected_string));

  const SerdURIView result   = serd_parse_uri(result_string.data);
  const SerdURIView expected = serd_parse_uri(expected_string);
  assert(component_equals(result.scheme, expected.scheme));
  assert(component_equals(result.authority, expected.authority));
  assert(component_equals(result.path_prefix, expected.path_prefix));
  assert(component_equals(result.path, expected.path));
  assert(component_equals(result.query, expected.query));
  assert(component_equals(result.fragment, expected.fragment));

  zix_free(NULL, result_string.data);
}

static void
test_relative_uri(void)
{
  // URI is already relative (NOOP)
  check_relative("a/b", "http://example.org/", "a/b");

  // Base is already relative (NOOP)
  check_relative("http://example.org/a/b", "a/", "http://example.org/a/b");

  // Base is only a scheme (scheme-relative possible but not implemented)
  check_relative("http://example.org/a/b", "http:", "http://example.org/a/b");

  // Unrelated scheme (NOOP)
  check_relative(
    "http://example.org/a/b", "ftp://example.org/", "http://example.org/a/b");

  // Unrelated authority (scheme-relative possible but not implemented)
  check_relative(
    "http://example.org/a/b", "http://example.com/", "http://example.org/a/b");

  // Related base
  check_relative("http://example.org/a/b", "http://example.org/", "a/b");
  check_relative("http://example.org/a/b", "http://example.org/a/", "b");
  check_relative("http://example.org/a/b", "http://example.org/a/b", "");
  check_relative("http://example.org/a/b", "http://example.org/a/b/", "../b");
  check_relative("http://example.org/a/b/", "http://example.org/a/b/", "");
  check_relative("http://example.org/", "http://example.org/", "");
  check_relative("http://example.org/", "http://example.org/a", "");
  check_relative("http://example.org/", "http://example.org/a/", "../");
  check_relative("http://example.org/", "http://example.org/a/b", "../");
  check_relative("http://example.org/", "http://example.org/a/b/", "../../");

  // Relative queries and fragments
  check_relative("http://example.org/p?q", "http://example.org/p", "?q");
  check_relative("http://example.org/p#f", "http://example.org/p", "#f");
  check_relative("http://example.org/p?q#f", "http://example.org/p", "?q#f");
}

static void
check_uri_string(const SerdURIView uri, const char* const expected)
{
  const SerdString string = serd_uri_to_string(NULL, uri);
  assert(expect_string_view(serd_string_view(string), expected));
  zix_free(NULL, string.data);
}

static void
check_resolve(const char* const r, const char* const base, const char* expected)
{
  check_uri_string(serd_resolve_uri(serd_parse_uri(r), serd_parse_uri(base)),
                   expected);
}

static void
test_resolve_uri(void)
{
  // Relative base (NOOP)
  check_resolve("rel", "/base", "rel");

  // Pathless base (additional slash)
  check_resolve("rel", "http://example.org", "http://example.org/rel");
  check_resolve("/", "http://example.org", "http://example.org/");

  // Scheme ... (absolute, NOOP)
  check_resolve("http://example.org/", "ftp:", "http://example.org/");
  check_resolve("urn:ietf:rfc:3986", "http://", "urn:ietf:rfc:3986");

  // Authority ...
  check_resolve("//example.org", "http:", "http://example.org");
  check_resolve("//example.org?q", "http:", "http://example.org?q");
  check_resolve("//example.org#f", "http:", "http://example.org#f");
  check_resolve("//example.org?q#f", "http:", "http://example.org?q#f");
  check_resolve("//example.org/", "http:", "http://example.org/");
  check_resolve("//example.org/?q", "http:", "http://example.org/?q");
  check_resolve("//example.org/#f", "http:", "http://example.org/#f");
  check_resolve("//example.org/?q#f", "http:", "http://example.org/?q#f");
  check_resolve("//example.com/", "http://example.org/", "http://example.com/");

  // Path ...
  check_resolve("/p", "http://example.org/b", "http://example.org/p");
  check_resolve("/p", "http://example.org/b/", "http://example.org/p");
  check_resolve("/p?q", "http://example.org/b/", "http://example.org/p?q");
  check_resolve("/p#f", "http://example.org/b/", "http://example.org/p#f");
  check_resolve("/p?q#f", "http://example.org/b/", "http://example.org/p?q#f");
  check_resolve("p", "http://example.org/b", "http://example.org/p");
  check_resolve("p?q", "http://example.org/b", "http://example.org/p?q");
  check_resolve("p#f", "http://example.org/b", "http://example.org/p#f");
  check_resolve("p?q#f", "http://example.org/b", "http://example.org/p?q#f");
  check_resolve("p", "http://example.org", "http://example.org/p");
  check_resolve("p", "http://example.org/b/", "http://example.org/b/p");
  check_resolve("p?q", "http://example.org/b/", "http://example.org/b/p?q");
  check_resolve("p#f", "http://example.org/b/", "http://example.org/b/p#f");
  check_resolve("p?q#f", "http://example.org/b/", "http://example.org/b/p?q#f");

  // Path ... (with dots)
  check_resolve(".", "http://example.org/b/c/", "http://example.org/b/c/");
  check_resolve("..", "http://example.org/b/c/", "http://example.org/b/");
  check_resolve("/./p", "http://example.org/b/c/", "http://example.org/p");
  check_resolve("/../p", "http://example.org/b/c/", "http://example.org/p");
  check_resolve("./p", "http://example.org/b/c/", "http://example.org/b/c/p");
  check_resolve("../p", "http://example.org/b/c/", "http://example.org/b/p");
  check_resolve("../../p", "http://example.org/b/c/", "http://example.org/p");

  // Path ... (file base)
  check_resolve("p", "file:", "file:p");
  check_resolve("p", "file://", "file://p");
  check_resolve("p", "file:///", "file:///p");
  check_resolve("p", "file://h/", "file://h/p");
  check_resolve("p", "file://h/b/", "file://h/b/p");

  // Query ...
  check_resolve("?q", "http://example.org/b", "http://example.org/b?q");
  check_resolve("?q#f", "http://example.org/b", "http://example.org/b?q#f");

  // Fragment ...
  check_resolve("#f", "http://example.org/b", "http://example.org/b#f");
  check_resolve("#f", "http://example.org/b?q", "http://example.org/b?q#f");
}

static void
check_relative_resolved(const char* const rel_str,
                        const char* const old_base_str,
                        const char* const new_base_str,
                        const char* const expected_str)
{
  const SerdURIView rel_uri      = serd_parse_uri(rel_str);
  const SerdURIView old_base_uri = serd_parse_uri(old_base_str);
  const SerdURIView new_base_uri = serd_parse_uri(new_base_str);
  const SerdURIView resolved     = serd_resolve_uri(rel_uri, old_base_uri);
  const SerdURIView result       = serd_relative_uri(resolved, new_base_uri);

  if (expected_str) {
    check_uri_string(result, expected_str);
  } else {
    assert(!result.scheme.data);
    assert(!result.scheme.length);
    assert(!result.authority.data);
    assert(!result.authority.length);
    assert(!result.path_prefix.data);
    assert(!result.path_prefix.length);
    assert(!result.path.data);
    assert(!result.path.length);
    assert(!result.query.data);
    assert(!result.query.length);
    assert(!result.fragment.data);
    assert(!result.fragment.length);
  }
}

static void
test_relative_resolved(void)
{
  check_relative_resolved(
    "http://example.org/b/r?q#f", "http:", "http://example.org/o", "b/r?q#f");

  check_relative_resolved(
    "//example.org/b/r?q#f", "http:", "http://example.org/o", "b/r?q#f");

  check_relative_resolved(
    "/b/r?q#f", "http://example.org/", "http://example.org/o", "b/r?q#f");

  check_relative_resolved(
    "?q#f", "http://example.org/b/r", "http://example.org/o", "b/r?q#f");

  check_relative_resolved(
    "#f", "http://example.org/b/r?q", "http://example.org/o", "b/r?q#f");

  // Shared path prefix is within URI path prefix
  check_relative_resolved(
    "r?q#f", "http://example.org/b/", "http://example.org/o", "b/r?q#f");

  // Failure to resolve because up-reference escapes path prefix
  check_relative_resolved(
    "r", "http://example.org/b/", "http://example.org/b/r/d", NULL);

  // Resolved input matches new base
  check_relative_resolved(
    "r/", "http://example.org/b/", "http://example.org/b/r/", "");
}

int
main(void)
{
  test_uri_string_has_scheme();
  test_uri_string_length();
  test_parse_uri();
  test_uri_to_string();
  test_is_within();
  test_relative_uri();
  test_resolve_uri();
  test_relative_resolved();
  return 0;
}
