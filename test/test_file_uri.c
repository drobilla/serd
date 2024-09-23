// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/file_uri.h>
#include <serd/string.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <string.h>

static void
test_file_uri_failed_alloc(void)
{
  static const char* const string = "file://host/path/spacey%20dir/100%25.ttl";

  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully parse a URI to count the number of allocations
  char* hostname = NULL;
  char* path     = serd_parse_file_uri(&allocator.base, string, &hostname);

  assert(expect_string(path, "/path/spacey dir/100%.ttl"));
  assert(expect_string(hostname, "host"));
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
check_file_uri(const char* const hostname,
               const char* const path,
               const char* const expected_uri,
               const char*       expected_path)
{
  if (!expected_path) {
    expected_path = path;
  }

  SerdString string =
    serd_file_uri_to_string(NULL, zix_string(path), zix_string(hostname));

  char* out_hostname = NULL;
  char* out_path     = serd_parse_file_uri(NULL, string.data, &out_hostname);
  assert(out_path);
  assert(expect_string_view(serd_string_view(string), expected_uri));
  assert((hostname && out_hostname) || (!hostname && !out_hostname));
  assert(!hostname || expect_string(hostname, out_hostname));
  assert(expect_string(out_path, expected_path));

  zix_free(NULL, out_path);
  zix_free(NULL, out_hostname);
  zix_free(NULL, string.data);
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
  assert(hosted);
  assert(expect_string(hosted, "/path"));
  zix_free(NULL, hosted);

  // Test missing trailing '/' after authority
  assert(!serd_parse_file_uri(NULL, "file://truncated", NULL));

  // Test rejection of invalid percent-encoding
  assert(!serd_parse_file_uri(NULL, "file:///dir/%X0", NULL));
  assert(!serd_parse_file_uri(NULL, "file:///dir/%0X", NULL));
  assert(!serd_parse_file_uri(NULL, "file:///dir/100%%", NULL));
}

int
main(void)
{
  test_file_uri_failed_alloc();
  test_file_uri();
  return 0;
}
