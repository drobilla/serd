// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/node.h>
#include <serd/node_type.h>
#include <serd/uri.h>
#include <zix/allocator.h>

#include <assert.h>
#include <stdbool.h>
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
  const size_t n_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);

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

static void
check_file_uri(const char* const hostname,
               const char* const path,
               const char* const expected_uri,
               const char*       expected_path)
{
  if (!expected_path) {
    expected_path = path;
  }

  SerdNode node         = serd_node_new_file_uri(NULL, path, hostname);
  char*    out_hostname = NULL;
  char*    out_path =
    serd_parse_file_uri(NULL, (const char*)node.buf, &out_hostname);
  assert(expect_string(node.buf, expected_uri));
  assert((hostname && out_hostname) || (!hostname && !out_hostname));
  assert(!hostname || expect_string(hostname, out_hostname));
  assert(expect_string(out_path, expected_path));

  zix_free(NULL, out_path);
  zix_free(NULL, out_hostname);
  serd_node_free(NULL, &node);
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
  assert(expect_string(hosted, "/path"));
  zix_free(NULL, hosted);

  // Test missing trailing '/' after authority
  assert(!serd_parse_file_uri(NULL, "file://truncated", NULL));

  // Test rejection of invalid percent-encoding
  assert(!serd_parse_file_uri(NULL, "file:///dir/%X0", NULL));
  assert(!serd_parse_file_uri(NULL, "file:///dir/%0X", NULL));
  assert(!serd_parse_file_uri(NULL, "file:///dir/100%%", NULL));
}

static void
test_uri_from_string(void)
{
  SerdNode nonsense = serd_node_new_uri_from_string(NULL, NULL);
  assert(nonsense.type == SERD_NOTHING);

  SerdNode base = serd_node_new_uri_from_string(NULL, "http://example.org/");
  SerdNode nil1 = serd_node_new_uri_from_string(NULL, NULL);
  SerdNode nil2 = serd_node_new_uri_from_string(NULL, "");
  assert(!nil1.type);
  assert(!nil1.buf);
  assert(!nil2.type);
  assert(!nil2.buf);
  serd_node_free(NULL, &nil2);
  serd_node_free(NULL, &nil1);
  serd_node_free(NULL, &base);
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

static bool
component_equals(const SerdURIComponent* const a,
                 const SerdURIComponent* const b)
{
  return (!a->length && !b->length && !a->data && !b->data) ||
         (a->length && b->length && a->data && b->data &&
          !strncmp(a->data, b->data, a->length));
}

static void
check_relative(const char* const uri_string,
               const char* const base_string,
               const char* const expected_string)
{
  assert(uri_string);
  assert(base_string);
  assert(expected_string);

  SerdNode uri_node  = serd_node_new_uri_from_string(NULL, uri_string);
  SerdNode base_node = serd_node_new_uri_from_string(NULL, base_string);
  assert(uri_node.buf);
  assert(base_node.buf);

  const SerdURIView uri  = serd_parse_uri(uri_node.buf);
  const SerdURIView base = serd_parse_uri(base_node.buf);

  const SerdURIView rel         = serd_relative_uri(uri, base);
  SerdNode          result_node = serd_node_new_uri(NULL, &rel);

  assert(expect_string(result_node.buf, expected_string));

  const SerdURIView result   = serd_parse_uri(result_node.buf);
  const SerdURIView expected = serd_parse_uri(expected_string);
  assert(component_equals(&result.scheme, &expected.scheme));
  assert(component_equals(&result.authority, &expected.authority));
  assert(component_equals(&result.path_prefix, &expected.path_prefix));
  assert(component_equals(&result.path, &expected.path));
  assert(component_equals(&result.query, &expected.query));
  assert(component_equals(&result.fragment, &expected.fragment));

  serd_node_free(NULL, &result_node);
  serd_node_free(NULL, &base_node);
  serd_node_free(NULL, &uri_node);
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
  SerdNode node = serd_node_new_uri(NULL, &uri);
  assert(expect_string(node.buf, expected));
  serd_node_free(NULL, &node);
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
  test_file_uri_failed_alloc();
  test_uri_string_has_scheme();
  test_uri_string_length();
  test_file_uri();
  test_uri_from_string();
  test_is_within();
  test_relative_uri();
  test_resolve_uri();
  test_relative_resolved();
  return 0;
}
