/*
  Copyright 2011-2019 David Robillard <http://drobilla.net>

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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int
test_round_trip(void)
{
	for (size_t size = 1; size < 1024; ++size) {
		const size_t len = serd_base64_encoded_length(size, true);

		char* buf = (char*)malloc(size);
		for (size_t i = 0; i < size; ++i) {
			buf[i] = (char)i;
		}

		char* str = (char*)calloc(1, len + 1);
		serd_base64_encode(str, buf, size, true);

		const size_t max_size  = serd_base64_decoded_size(len);
		size_t       copy_size = 0;
		char*        copy      = (char*)malloc(max_size);
		serd_base64_decode(copy, &copy_size, str, len);
		assert(copy_size == size);
		assert(!memcmp(buf, copy, size));

		free(copy);
		free(str);
		free(buf);
	}

	return 0;
}

static void
test_encoding_equals(const char* const input, const char* const expected)
{
	const size_t size = strlen(input);
	const size_t len  = serd_base64_encoded_length(size, true);

	char* str = (char*)calloc(1, len + 1);
	serd_base64_encode(str, input, size, true);

	assert(!strcmp(str, expected));

	free(str);
}

static int
test_rfc4648_vectors(void)
{
	test_encoding_equals("f", "Zg==");
	test_encoding_equals("fo", "Zm8=");
	test_encoding_equals("foo", "Zm9v");
	test_encoding_equals("foob", "Zm9vYg==");
	test_encoding_equals("fooba", "Zm9vYmE=");
	test_encoding_equals("foobar", "Zm9vYmFy");
	return 0;
}

static void
test_decoding_equals(const char* const base64, const char* const expected)
{
	const size_t len  = strlen(base64);
	const size_t size = serd_base64_decoded_size(len);

	size_t buf_size = 0;
	char*  buf      = (char*)malloc(size);
	serd_base64_decode(buf, &buf_size, base64, len);

	assert(buf_size <= size);
	assert(!memcmp(buf, expected, buf_size));

	free(buf);
}

static int
test_junk(void)
{
	test_decoding_equals("?Zm9vYmFy", "foobar");
	test_decoding_equals("Z?m9vYmFy", "foobar");
	test_decoding_equals("?Z?m9vYmFy", "foobar");
	test_decoding_equals("?Z??m9vYmFy", "foobar");
	test_decoding_equals("?Z???m9vYmFy", "foobar");
	test_decoding_equals("?Z????m9vYmFy", "foobar");

	test_decoding_equals("Zm9vYmFy?", "foobar");
	test_decoding_equals("Zm9vYmF?y?", "foobar");
	test_decoding_equals("Zm9vYmF?y??", "foobar");
	test_decoding_equals("Zm9vYmF?y???", "foobar");
	test_decoding_equals("Zm9vYmF?y????", "foobar");

	return 0;
}

int
main(void)
{
	return test_round_trip() || test_rfc4648_vectors() || test_junk();
}
