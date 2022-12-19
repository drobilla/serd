// Copyright 2011-2021 David Robillard <d@drobilla.net>
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
  static const char* const string = "file://host/path/spacey%20dir/100%%.ttl";

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
test_file_uri(const char* const hostname,
              const char* const path,
              const char* const expected_uri,
              const char*       expected_path)
{
  if (!expected_path) {
    expected_path = path;
  }

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNode* node = serd_nodes_get(
    nodes, serd_a_file_uri(zix_string(path), zix_optional_string(hostname)));

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
test_uri_parsing(void)
{
  test_file_uri(NULL, "C:/My 100%", "file:///C:/My%20100%%", NULL);
  test_file_uri(NULL, "/foo/bar", "file:///foo/bar", NULL);
  test_file_uri("bhost", "/foo/bar", "file://bhost/foo/bar", NULL);
  test_file_uri(NULL, "a/relative <path>", "a/relative%20%3Cpath%3E", NULL);

#ifdef _WIN32
  test_file_uri(NULL, "C:\\My 100%", "file:///C:/My%20100%%", "C:/My 100%");

  test_file_uri(
    NULL, "\\drive\\relative", "file:///drive/relative", "/drive/relative");

  test_file_uri(NULL,
                "C:\\Program Files\\Serd",
                "file:///C:/Program%20Files/Serd",
                "C:/Program Files/Serd");

  test_file_uri("ahost",
                "C:\\Pointless Space",
                "file://ahost/C:/Pointless%20Space",
                "C:/Pointless Space");
#else
  /* What happens with Windows paths on other platforms is a bit weird, but
     more or less unavoidable.  It doesn't work to interpret backslashes as
     path separators on any other platform. */

  test_file_uri("ahost",
                "C:\\Pointless Space",
                "file://ahost/C:%5CPointless%20Space",
                "/C:\\Pointless Space");

  test_file_uri(
    NULL, "\\drive\\relative", "%5Cdrive%5Crelative", "\\drive\\relative");

  test_file_uri(NULL,
                "C:\\Program Files\\Serd",
                "file:///C:%5CProgram%20Files%5CSerd",
                "/C:\\Program Files\\Serd");

  test_file_uri("ahost",
                "C:\\Pointless Space",
                "file://ahost/C:%5CPointless%20Space",
                "/C:\\Pointless Space");
#endif

  // Missing trailing '/' after authority
  assert(!serd_parse_file_uri(NULL, "file://truncated", NULL));

  // Check that NULL hostname doesn't crash
  char* out_path = serd_parse_file_uri(NULL, "file://me/path", NULL);
  assert(!strcmp(out_path, "/path"));
  zix_free(NULL, out_path);

  // Invalid first escape character
  out_path = serd_parse_file_uri(NULL, "file:///foo/%0Xbar", NULL);
  assert(!strcmp(out_path, "/foo/bar"));
  zix_free(NULL, out_path);

  // Invalid second escape character
  out_path = serd_parse_file_uri(NULL, "file:///foo/%X0bar", NULL);
  assert(!strcmp(out_path, "/foo/bar"));
  zix_free(NULL, out_path);
}

static void
test_parse_uri(void)
{
  const ZixStringView base = zix_string("http://example.org/a/b/c/");

  const SerdURIView base_uri  = serd_parse_uri(base.data);
  const SerdURIView empty_uri = serd_parse_uri("");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNode* const nil = serd_nodes_get(
    nodes, serd_a_parsed_uri(serd_resolve_uri(empty_uri, base_uri)));

  assert(serd_node_type(nil) == SERD_URI);
  assert(!strcmp(serd_node_string(nil), base.data));

  serd_nodes_free(nodes);
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
check_rel_uri(const char*     uri_string,
              const SerdNode* base,
              const SerdNode* root,
              const char*     expected)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdURIView base_uri = serd_node_uri_view(base);
  const SerdURIView uri      = serd_parse_uri(uri_string);
  const bool        is_within =
    !root || serd_uri_is_within(uri, serd_node_uri_view(root));

  const SerdNode* const rel =
    is_within ? serd_nodes_get(
                  nodes, serd_a_parsed_uri(serd_relative_uri(uri, base_uri)))
              : serd_nodes_get(nodes, serd_a_uri_string(uri_string));

  const int ret = strcmp(serd_node_string(rel), expected);
  assert(!ret);

  serd_nodes_free(nodes);
}

static void
test_relative_uri(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNode* const root =
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/a/b/ignored"));

  const SerdNode* const base =
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/a/b/c/"));

  check_rel_uri("http://example.org/a/b/c/foo", base, NULL, "foo");
  check_rel_uri("http://example.org/a/", base, NULL, "../../");
  check_rel_uri("http://example.org/a/", base, root, "http://example.org/a/");
  check_rel_uri("http://example.org/a/b/x", root, root, "x");
  check_rel_uri("http://example.org/", base, NULL, "../../../");
  check_rel_uri("http://drobilla.net/a", base, NULL, "http://drobilla.net/a");

  {
    // Check making a relative URI from a resolved URI
    const SerdURIView ref = serd_parse_uri("child");
    const SerdURIView abs = serd_resolve_uri(ref, serd_node_uri_view(base));
    const SerdURIView rel = serd_relative_uri(abs, serd_node_uri_view(root));
    const SerdNode* const node = serd_nodes_get(nodes, serd_a_parsed_uri(rel));

    assert(!strcmp(serd_node_string(node), "c/child"));
  }
  {
    // Check failure when path_prefix is not available for use
    const SerdURIView top   = serd_parse_uri("http://example.org/a/");
    const SerdURIView ref   = serd_parse_uri("up");
    const SerdURIView up    = serd_resolve_uri(ref, top);
    const SerdURIView upref = serd_relative_uri(up, serd_node_uri_view(base));

    assert(!memcmp(&upref, &SERD_URI_NULL, sizeof(ref)));
  }

  serd_nodes_free(nodes);
}

static void
test_uri_resolution(void)
{
  const ZixStringView base     = zix_string("http://example.org/a/b/c/");
  const ZixStringView base_foo = zix_string("http://example.org/a/b/c/foo");

  const SerdURIView base_uri     = serd_parse_uri(base.data);
  const SerdURIView abs_foo_uri  = serd_parse_uri(base_foo.data);
  const SerdURIView rel_foo_uri  = serd_relative_uri(abs_foo_uri, base_uri);
  const SerdURIView resolved_uri = serd_resolve_uri(rel_foo_uri, base_uri);

  SerdNodes* const      nodes = serd_nodes_new(NULL);
  const SerdNode* const resolved =
    serd_nodes_get(nodes, serd_a_parsed_uri(resolved_uri));

  assert(!strcmp(serd_node_string(resolved), "http://example.org/a/b/c/foo"));

  serd_nodes_free(nodes);
}

int
main(void)
{
  test_file_uri_failed_alloc();
  test_uri_string_has_scheme();
  test_uri_parsing();
  test_parse_uri();
  test_is_within();
  test_relative_uri();
  test_uri_resolution();

  printf("Success\n");
  return 0;
}
