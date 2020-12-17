/*
  Copyright 2011-2020 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

static void
test_file_uri(const char* hostname,
              const char* path,
              bool        escape,
              const char* expected_uri,
              const char* expected_path)
{
	if (!expected_path) {
		expected_path = path;
	}

	SerdNode node = serd_node_new_file_uri(
		USTR(path), USTR(hostname), 0, escape);

	uint8_t* out_hostname = NULL;
	uint8_t* out_path     = serd_file_uri_parse(node.buf, &out_hostname);
	assert(!strcmp((const char*)node.buf, expected_uri));
	assert((hostname && out_hostname) || (!hostname && !out_hostname));
	assert(!strcmp((const char*)out_path, (const char*)expected_path));

	serd_free(out_path);
	serd_free(out_hostname);
	serd_node_free(&node);
}

#if defined(__GNUC__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static void
test_uri_to_path(void)
{
	const uint8_t* uri = (const uint8_t*)"file:///home/user/foo.ttl";
	assert(!strcmp((const char*)serd_uri_to_path(uri), "/home/user/foo.ttl"));

	uri = (const uint8_t*)"file://localhost/home/user/foo.ttl";
	assert(!strcmp((const char*)serd_uri_to_path(uri), "/home/user/foo.ttl"));

	uri = (const uint8_t*)"file:illegal/file/uri";
	assert(!serd_uri_to_path(uri));

	uri = (const uint8_t*)"file:///c:/awful/system";
	assert(!strcmp((const char*)serd_uri_to_path(uri), "c:/awful/system"));

	uri = (const uint8_t*)"file:///c:awful/system";
	assert(!strcmp((const char*)serd_uri_to_path(uri), "/c:awful/system"));

	uri = (const uint8_t*)"file:///0/1";
	assert(!strcmp((const char*)serd_uri_to_path(uri), "/0/1"));

	uri = (const uint8_t*)"C:\\Windows\\Sucks";
	assert(!strcmp((const char*)serd_uri_to_path(uri), "C:\\Windows\\Sucks"));

	uri = (const uint8_t*)"C|/Windows/Sucks";
	assert(!strcmp((const char*)serd_uri_to_path(uri), "C|/Windows/Sucks"));

	uri = (const uint8_t*)"http://example.org/path";
	assert(!serd_uri_to_path(uri));
}

#if defined(__GNUC__)
#	pragma GCC diagnostic pop
#endif

static void
test_uri_parsing(void)
{
	test_file_uri(NULL, "C:/My 100%", true,
	              "file:///C:/My%20100%%", NULL);
	test_file_uri("ahost", "C:\\Pointless Space", true,
	              "file://ahost/C:/Pointless%20Space", "C:/Pointless Space");
	test_file_uri(NULL, "/foo/bar", true,
	              "file:///foo/bar", NULL);
	test_file_uri("bhost", "/foo/bar", true,
	              "file://bhost/foo/bar", NULL);
	test_file_uri(NULL, "a/relative path", false,
	              "a/relative path", NULL);
	test_file_uri(NULL, "a/relative <path>", true,
	              "a/relative%20%3Cpath%3E", NULL);

	// Test tolerance of parsing junk URI escapes

	uint8_t* out_path = serd_file_uri_parse(USTR("file:///foo/%0Xbar"), NULL);
	assert(!strcmp((const char*)out_path, "/foo/bar"));
	serd_free(out_path);
}

static void
test_uri_from_string(void)
{
	SerdNode nonsense = serd_node_new_uri_from_string(NULL, NULL, NULL);
	assert(nonsense.type == SERD_NOTHING);

	SerdURI base_uri;
	SerdNode base = serd_node_new_uri_from_string(USTR("http://example.org/"),
	                                              NULL, &base_uri);
	SerdNode nil = serd_node_new_uri_from_string(NULL, &base_uri, NULL);
	SerdNode nil2 = serd_node_new_uri_from_string(USTR(""), &base_uri, NULL);
	assert(nil.type == SERD_URI);
	assert(!strcmp((const char*)nil.buf, (const char*)base.buf));
	assert(nil2.type == SERD_URI);
	assert(!strcmp((const char*)nil2.buf, (const char*)base.buf));
	serd_node_free(&nil);
	serd_node_free(&nil2);

	serd_node_free(&base);
}

static void
test_relative_uri(void)
{
	SerdURI base_uri;
	SerdNode base = serd_node_new_uri_from_string(USTR("http://example.org/"),
	                                              NULL, &base_uri);

	SerdNode abs = serd_node_from_string(SERD_URI, USTR("http://example.org/foo/bar"));
	SerdURI  abs_uri;
	serd_uri_parse(abs.buf, &abs_uri);

	SerdURI  rel_uri;
	SerdNode rel = serd_node_new_relative_uri(&abs_uri, &base_uri, NULL, &rel_uri);
	assert(!strcmp((const char*)rel.buf, "/foo/bar"));

	SerdNode up = serd_node_new_relative_uri(&base_uri, &abs_uri, NULL, NULL);
	assert(!strcmp((const char*)up.buf, "../"));

	SerdNode noup = serd_node_new_relative_uri(&base_uri, &abs_uri, &abs_uri, NULL);
	assert(!strcmp((const char*)noup.buf, "http://example.org/"));

	SerdNode x = serd_node_from_string(SERD_URI, USTR("http://example.org/foo/x"));
	SerdURI  x_uri;
	serd_uri_parse(x.buf, &x_uri);

	SerdNode x_rel = serd_node_new_relative_uri(&x_uri, &abs_uri, &abs_uri, NULL);
	assert(!strcmp((const char*)x_rel.buf, "x"));

	serd_node_free(&x_rel);
	serd_node_free(&noup);
	serd_node_free(&up);
	serd_node_free(&rel);
	serd_node_free(&base);
}

int
main(void)
{
	test_uri_to_path();
	test_uri_parsing();
	test_uri_from_string();
	test_relative_uri();

	printf("Success\n");
	return 0;
}
