// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/uri.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void
test_file_uri_failed_alloc(void)
{
  static const char* const string = "file://host/path/spacey%20dir/100%25.ttl";

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully parse a URI to count the number of allocations
  char* hostname = NULL;
  char* path     = serd_parse_file_uri(&allocator.base, string, &hostname);

  assert(!strcmp(path, "/path/spacey dir/100%.ttl"));
  assert(!strcmp(hostname, "host"));
  zix_free(&allocator.base, path);
  zix_free(&allocator.base, hostname);

  // Test that each allocation failing is handled gracefully
  const size_t n_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_allocs; ++i) {
    allocator.n_remaining = i;

    path = serd_parse_file_uri(&allocator.base, string, &hostname);
    assert(!path || !hostname);

    zix_free(&allocator.base, path);
    zix_free(&allocator.base, hostname);
  }
}

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
  assert(serd_uri_string_length(serd_parse_uri("http:")) == 5);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org")) == 18);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org/p")) == 20);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org?q")) == 20);
  assert(serd_uri_string_length(serd_parse_uri("http://example.org#f")) == 20);

  const SerdURIView needs_slash =
    serd_resolve_uri(serd_parse_uri("p"), serd_parse_uri("http://example.org"));

  assert(serd_uri_string_length(needs_slash) == 20);
}

static void
check_file_uri(const char* const hostname,
               const char* const path,
               const char* const expected_uri,
               const char*       expected_path)
{
  if (!expected_path) {
    expected_path = path;
  }

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNode* node = serd_nodes_get(
    nodes, serd_a_file_uri(zix_string(path), zix_string(hostname)));

  const char* node_str     = serd_node_string(node);
  char*       out_hostname = NULL;
  char* const out_path     = serd_parse_file_uri(NULL, node_str, &out_hostname);

  assert(!strcmp(node_str, expected_uri));
  assert((hostname && out_hostname) || (!hostname && !out_hostname));
  assert(!hostname || !strcmp(hostname, out_hostname));
  assert(!strcmp(out_path, expected_path));

  zix_free(NULL, out_path);
  zix_free(NULL, out_hostname);
  serd_nodes_free(nodes);
}

static void
test_file_uri(void)
{
  check_file_uri(NULL, "C:/My Documents", "file:///C:/My%20Documents", NULL);
  check_file_uri(NULL, "/foo/bar", "file:///foo/bar", NULL);
  check_file_uri("bhost", "/foo/bar", "file://bhost/foo/bar", NULL);
  check_file_uri(NULL, "a/relative <path>", "a/relative%20%3Cpath%3E", NULL);

#ifdef _WIN32
  check_file_uri(
    NULL, "C:\\My Documents", "file:///C:/My%20Documents", "C:/My Documents");

  check_file_uri(
    NULL, "\\drive\\relative", "file:///drive/relative", "/drive/relative");

  check_file_uri(NULL,
                 "C:\\Program Files\\Serd",
                 "file:///C:/Program%20Files/Serd",
                 "C:/Program Files/Serd");

  check_file_uri("ahost",
                 "C:\\Pointless Space",
                 "file://ahost/C:/Pointless%20Space",
                 "C:/Pointless Space");
#else
  /* What happens with Windows paths on other platforms is a bit weird, but
     more or less unavoidable.  It doesn't work to interpret backslashes as
     path separators on any other platform. */

  check_file_uri("ahost",
                 "C:\\Pointless Space",
                 "file://ahost/C:%5CPointless%20Space",
                 "/C:\\Pointless Space");

  check_file_uri(
    NULL, "\\drive\\relative", "%5Cdrive%5Crelative", "\\drive\\relative");

  check_file_uri(NULL,
                 "C:\\Program Files\\Serd",
                 "file:///C:%5CProgram%20Files%5CSerd",
                 "/C:\\Program Files\\Serd");

  check_file_uri("ahost",
                 "C:\\Pointless Space",
                 "file://ahost/C:%5CPointless%20Space",
                 "/C:\\Pointless Space");
#endif

  // Test tolerance of NULL hostname parameter
  char* const hosted = serd_parse_file_uri(NULL, "file://host/path", NULL);
  assert(!strcmp(hosted, "/path"));
  zix_free(NULL, hosted);

  // Test rejection of invalid percent-encoding
  assert(!serd_parse_file_uri(NULL, "file:///dir/%X0", NULL));
  assert(!serd_parse_file_uri(NULL, "file:///dir/%0X", NULL));
  assert(!serd_parse_file_uri(NULL, "file:///dir/100%%", NULL));

  // Test missing trailing '/' after authority
  assert(!serd_parse_file_uri(NULL, "file://truncated", NULL));
}

static inline bool
chunk_equals(const ZixStringView a, const ZixStringView b)
{
  return (!a.length && !b.length) ||
         (a.length && b.length && a.data && b.data &&
          !strncmp((const char*)a.data, (const char*)b.data, a.length));
}

static bool
component_matches(const ZixStringView actual, const char* const expected)
{
  return ((!expected && !actual.data && !actual.length) ||
          chunk_equals(zix_string(expected), actual));
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

  // Check that a node can be created from the parsed URI with the same string
  SerdNode* const node = serd_node_new(NULL, serd_a_parsed_uri(uri));
  assert(serd_node_type(node) == SERD_URI);
  assert(!strcmp(serd_node_string(node), uri_string));
  serd_node_free(NULL, node);
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

  // Check that a node can be created from the parsed URI with the same string
  const SerdURIView noslash_uri = {zix_string("http"),
                                   zix_string("example.org"),
                                   zix_string(""),
                                   zix_string("noslash"),
                                   zix_string("q"),
                                   zix_string("#f")};
  SerdNode* const   noslash_node =
    serd_node_new(NULL, serd_a_parsed_uri(noslash_uri));
  assert(serd_node_type(noslash_node) == SERD_URI);
  assert(
    !strcmp(serd_node_string(noslash_node), "http://example.org/noslash?q#f"));
  serd_node_free(NULL, noslash_node);
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
check_relative_uri(const char* const uri_string,
                   const char* const base_string,
                   const char* const root_string,
                   const char* const expected_string)
{
  assert(uri_string);
  assert(base_string);
  assert(expected_string);

  SerdNodes* const nodes = serd_nodes_new(NULL);
  const SerdNode*  uri_node =
    serd_nodes_get(nodes, serd_a_uri_string(uri_string));
  const SerdURIView uri = serd_node_uri_view(uri_node);
  const SerdNode*   base_node =
    serd_nodes_get(nodes, serd_a_uri_string(base_string));
  const SerdURIView base = serd_node_uri_view(base_node);

  const SerdNode* result_node = NULL;
  if (!root_string) {
    result_node =
      serd_nodes_get(nodes, serd_a_parsed_uri(serd_relative_uri(uri, base)));
  } else {
    const SerdNode* root_node =
      serd_nodes_get(nodes, serd_a_uri_string(root_string));
    const SerdURIView root = serd_node_uri_view(root_node);

    result_node =
      serd_uri_is_within(uri, root)
        ? serd_nodes_get(nodes, serd_a_parsed_uri(serd_relative_uri(uri, base)))
        : serd_nodes_get(nodes, serd_a_uri_string(uri_string));
  }

  assert(!strcmp(serd_node_string(result_node), expected_string));

  const SerdURIView result   = serd_node_uri_view(result_node);
  const SerdURIView expected = serd_parse_uri(expected_string);
  assert(chunk_equals(result.scheme, expected.scheme));
  assert(chunk_equals(result.authority, expected.authority));
  assert(chunk_equals(result.path_prefix, expected.path_prefix));
  assert(chunk_equals(result.path, expected.path));
  assert(chunk_equals(result.query, expected.query));
  assert(chunk_equals(result.fragment, expected.fragment));

  serd_nodes_free(nodes);
}

static void
test_relative_uri(void)
{
  // Unrelated base

  check_relative_uri("http://example.org/a/b",
                     "ftp://example.org/",
                     NULL,
                     "http://example.org/a/b");

  check_relative_uri("http://example.org/a/b",
                     "http://example.com/",
                     NULL,
                     "http://example.org/a/b");

  // Related base

  check_relative_uri(
    "http://example.org/a/b", "http://example.org/", NULL, "a/b");

  check_relative_uri(
    "http://example.org/a/b", "http://example.org/a/", NULL, "b");

  check_relative_uri(
    "http://example.org/a/b", "http://example.org/a/b", NULL, "");

  check_relative_uri(
    "http://example.org/a/b", "http://example.org/a/b/", NULL, "../b");

  check_relative_uri(
    "http://example.org/a/b/", "http://example.org/a/b/", NULL, "");

  check_relative_uri("http://example.org/", "http://example.org/", NULL, "");

  check_relative_uri("http://example.org/", "http://example.org/a", NULL, "");

  check_relative_uri(
    "http://example.org/", "http://example.org/a/", NULL, "../");

  check_relative_uri(
    "http://example.org/", "http://example.org/a/b", NULL, "../");

  check_relative_uri(
    "http://example.org/", "http://example.org/a/b/", NULL, "../../");

  // Unrelated root

  check_relative_uri("http://example.org/",
                     "http://example.org/a/b",
                     "relative",
                     "http://example.org/");

  check_relative_uri("http://example.org/",
                     "http://example.org/a/b",
                     "ftp://example.org/",
                     "http://example.org/");

  check_relative_uri("http://example.org/",
                     "http://example.org/a/b",
                     "http://example.com/",
                     "http://example.org/");

  // Related root

  check_relative_uri("http://example.org/a/b",
                     "http://example.org/",
                     "http://example.org/c/d",
                     "http://example.org/a/b");

  check_relative_uri("http://example.org/",
                     "http://example.org/a/b",
                     "http://example.org/a/b",
                     "http://example.org/");

  check_relative_uri("http://example.org/a/b",
                     "http://example.org/a/b",
                     "http://example.org/a/b",
                     "");

  check_relative_uri("http://example.org/a/",
                     "http://example.org/a/",
                     "http://example.org/a/",
                     "");

  check_relative_uri("http://example.org/a/b",
                     "http://example.org/a/b/c",
                     "http://example.org/a/b",
                     "../b");

  check_relative_uri("http://example.org/a",
                     "http://example.org/a/b/c",
                     "http://example.org/a/b",
                     "http://example.org/a");
}

static void
check_uri_string(const SerdURIView uri, const char* const expected)
{
  SerdNode* const node = serd_node_new(NULL, serd_a_parsed_uri(uri));
  assert(!strcmp(serd_node_string(node), expected));
  serd_node_free(NULL, node);
}

static void
test_uri_resolution(void)
{
#define NS_EG "http://example.org/"

  static const char* const top_str   = NS_EG "t/";
  static const char* const base_str  = NS_EG "t/b/";
  static const char* const sub_str   = NS_EG "t/b/s";
  static const char* const deep_str  = NS_EG "t/b/s/d";
  static const char* const other_str = NS_EG "o";

  const SerdURIView top_uri          = serd_parse_uri(top_str);
  const SerdURIView base_uri         = serd_parse_uri(base_str);
  const SerdURIView sub_uri          = serd_parse_uri(sub_str);
  const SerdURIView deep_uri         = serd_parse_uri(deep_str);
  const SerdURIView other_uri        = serd_parse_uri(other_str);
  const SerdURIView rel_sub_uri      = serd_relative_uri(sub_uri, base_uri);
  const SerdURIView resolved_sub_uri = serd_resolve_uri(rel_sub_uri, base_uri);

  check_uri_string(top_uri, top_str);
  check_uri_string(base_uri, base_str);
  check_uri_string(sub_uri, sub_str);
  check_uri_string(deep_uri, deep_str);
  check_uri_string(other_uri, other_str);
  check_uri_string(rel_sub_uri, "s");
  check_uri_string(resolved_sub_uri, sub_str);

  // Failure to resolve because up-reference escapes path prefix
  const SerdURIView up_uri = serd_relative_uri(resolved_sub_uri, deep_uri);
  assert(!up_uri.scheme.data);
  assert(!up_uri.scheme.length);
  assert(!up_uri.authority.data);
  assert(!up_uri.authority.length);
  assert(!up_uri.path_prefix.data);
  assert(!up_uri.path_prefix.length);
  assert(!up_uri.path.data);
  assert(!up_uri.path.length);
  assert(!up_uri.query.data);
  assert(!up_uri.query.length);
  assert(!up_uri.fragment.data);
  assert(!up_uri.fragment.length);

  // Shared path prefix is within URI path prefix
  const SerdURIView prefix_uri = serd_relative_uri(resolved_sub_uri, other_uri);
  check_uri_string(prefix_uri, "t/b/s");

#undef NS_EG
}

int
main(void)
{
  test_file_uri_failed_alloc();
  test_uri_string_has_scheme();
  test_uri_string_length();
  test_file_uri();
  test_parse_uri();
  test_is_within();
  test_relative_uri();
  test_uri_resolution();

  printf("Success\n");
  return 0;
}
