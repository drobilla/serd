// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/memory.h"
#include "serd/node.h"
#include "serd/uri.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
test_file_uri(const char* hostname,
              const char* path,
              const char* expected_uri,
              const char* expected_path)
{
  if (!expected_path) {
    expected_path = path;
  }

  SerdNode*   node         = serd_new_file_uri(path, hostname, 0);
  const char* node_str     = serd_node_string(node);
  char*       out_hostname = NULL;
  char*       out_path     = serd_file_uri_parse(node_str, &out_hostname);
  assert(!strcmp(node_str, expected_uri));
  assert((hostname && out_hostname) || (!hostname && !out_hostname));
  assert(!strcmp(out_path, expected_path));

  serd_free(out_path);
  serd_free(out_hostname);
  serd_node_free(node);
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

  // Test tolerance of parsing junk URI escapes

  char* out_path = serd_file_uri_parse("file:///foo/%0Xbar", NULL);
  assert(!strcmp(out_path, "/foo/bar"));
  serd_free(out_path);
}

static void
test_uri_from_string(void)
{
  assert(!serd_new_uri_from_string(NULL, NULL, NULL));

  SerdURIView base_uri;
  SerdNode*   base =
    serd_new_uri_from_string("http://example.org/", NULL, &base_uri);
  SerdNode* nil  = serd_new_uri_from_string(NULL, &base_uri, NULL);
  SerdNode* nil2 = serd_new_uri_from_string("", &base_uri, NULL);
  assert(serd_node_type(nil) == SERD_URI);
  assert(!strcmp(serd_node_string(nil), serd_node_string(base)));
  assert(serd_node_type(nil2) == SERD_URI);
  assert(!strcmp(serd_node_string(nil2), serd_node_string(base)));
  serd_node_free(nil);
  serd_node_free(nil2);
  serd_node_free(base);
}

static void
test_relative_uri(void)
{
  SerdURIView base_uri;
  SerdNode*   base =
    serd_new_uri_from_string("http://example.org/", NULL, &base_uri);

  SerdNode*   abs = serd_new_string(SERD_URI, "http://example.org/foo/bar");
  SerdURIView abs_uri;
  serd_uri_parse(serd_node_string(abs), &abs_uri);

  SerdURIView rel_uri;
  SerdNode*   rel = serd_new_relative_uri(&abs_uri, &base_uri, NULL, &rel_uri);
  assert(!strcmp(serd_node_string(rel), "/foo/bar"));

  SerdNode* up = serd_new_relative_uri(&base_uri, &abs_uri, NULL, NULL);
  assert(!strcmp(serd_node_string(up), "../"));

  SerdNode* noup = serd_new_relative_uri(&base_uri, &abs_uri, &abs_uri, NULL);
  assert(!strcmp(serd_node_string(noup), "http://example.org/"));

  SerdNode*   x = serd_new_string(SERD_URI, "http://example.org/foo/x");
  SerdURIView x_uri;
  serd_uri_parse(serd_node_string(x), &x_uri);

  SerdNode* x_rel = serd_new_relative_uri(&x_uri, &abs_uri, &abs_uri, NULL);
  assert(!strcmp(serd_node_string(x_rel), "x"));

  serd_node_free(x_rel);
  serd_node_free(x);
  serd_node_free(noup);
  serd_node_free(up);
  serd_node_free(abs);
  serd_node_free(rel);
  serd_node_free(base);
}

int
main(void)
{
  test_uri_parsing();
  test_uri_from_string();
  test_relative_uri();

  printf("Success\n");
  return 0;
}
