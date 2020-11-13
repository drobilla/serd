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
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

#ifndef INFINITY
#    define INFINITY (DBL_MAX + DBL_MAX)
#endif
#ifndef NAN
#    define NAN (INFINITY - INFINITY)
#endif

static void
test_strtod(double dbl, double max_delta)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%f", dbl);

	char* endptr = NULL;
	const double out = serd_strtod(buf, &endptr);

	const double diff = fabs(out - dbl);
	assert(diff <= max_delta);
}

static void
test_string_to_double(void)
{
	const double expt_test_nums[] = {
		2.0E18, -5e19, +8e20, 2e+24, -5e-5, 8e0, 9e-0, 2e+0
	};

	const char* expt_test_strs[] = {
		"02e18", "-5e019", "+8e20", "2E+24", "-5E-5", "8E0", "9e-0", " 2e+0"
	};

	for (size_t i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
		const double num   = serd_strtod(expt_test_strs[i], NULL);
		const double delta = fabs(num - expt_test_nums[i]);
		assert(delta <= DBL_EPSILON);

		test_strtod(expt_test_nums[i], DBL_EPSILON);
	}
}

static void
test_double_to_node(void)
{
	const double dbl_test_nums[] = {
		0.0, 9.0, 10.0, .01, 2.05, -16.00001, 5.000000005, 0.0000000001, NAN, INFINITY
	};

	const char* dbl_test_strs[] = {
		"0.0", "9.0", "10.0", "0.01", "2.05", "-16.00001", "5.00000001", "0.0", NULL, NULL
	};

	for (size_t i = 0; i < sizeof(dbl_test_nums) / sizeof(double); ++i) {
		SerdNode   node = serd_node_new_decimal(dbl_test_nums[i], 8);
		const bool pass = (node.buf && dbl_test_strs[i])
			? !strcmp((const char*)node.buf, dbl_test_strs[i])
			: ((const char*)node.buf == dbl_test_strs[i]);
		assert(pass);
		const size_t len = node.buf ? strlen((const char*)node.buf) : 0;
		assert(node.n_bytes == len && node.n_chars == len);
		serd_node_free(&node);
	}
}

static void
test_integer_to_node(void)
{
	const long int_test_nums[] = {
		0, -0, -23, 23, -12340, 1000, -1000
	};

	const char* int_test_strs[] = {
		"0", "0", "-23", "23", "-12340", "1000", "-1000"
	};

	for (size_t i = 0; i < sizeof(int_test_nums) / sizeof(double); ++i) {
		SerdNode node = serd_node_new_integer(int_test_nums[i]);
		assert(!strcmp((const char*)node.buf, (const char*)int_test_strs[i]));
		const size_t len = strlen((const char*)node.buf);
		assert(node.n_bytes == len && node.n_chars == len);
		serd_node_free(&node);
	}
}

static void
test_blob_to_node(void)
{
	for (size_t size = 1; size < 256; ++size) {
		uint8_t* const data = (uint8_t*)malloc(size);
		for (size_t i = 0; i < size; ++i) {
			data[i] = (uint8_t)((size + i) % 256);
		}

		SerdNode blob = serd_node_new_blob(data, size, size % 5);

		assert(blob.n_bytes == blob.n_chars);
		assert(blob.n_bytes == strlen((const char*)blob.buf));

		size_t   out_size = 0;
		uint8_t* out = (uint8_t*)serd_base64_decode(
			blob.buf, blob.n_bytes, &out_size);
		assert(out_size == size);

		for (size_t i = 0; i < size; ++i) {
			assert(out[i] == data[i]);
		}

		serd_node_free(&blob);
		serd_free(out);
		free(data);
	}
}

static void
test_node_equals(void)
{
	const uint8_t replacement_char_str[] = { 0xEF, 0xBF, 0xBD, 0 };
	SerdNode lhs = serd_node_from_string(SERD_LITERAL, replacement_char_str);
	SerdNode rhs = serd_node_from_string(SERD_LITERAL, USTR("123"));
	assert(!serd_node_equals(&lhs, &rhs));

	SerdNode qnode = serd_node_from_string(SERD_CURIE, USTR("foo:bar"));
	assert(!serd_node_equals(&lhs, &qnode));
	assert(serd_node_equals(&lhs, &lhs));

	SerdNode null_copy = serd_node_copy(&SERD_NODE_NULL);
	assert(serd_node_equals(&SERD_NODE_NULL, &null_copy));
}

static void
test_node_from_string(void)
{
	SerdNode node = serd_node_from_string(SERD_LITERAL, (const uint8_t*)"hello\"");
	assert(node.n_bytes == 6 && node.n_chars == 6 &&
	       node.flags == SERD_HAS_QUOTE &&
	       !strcmp((const char*)node.buf, "hello\""));

	node = serd_node_from_string(SERD_URI, NULL);
	assert(serd_node_equals(&node, &SERD_NODE_NULL));
}

static void
test_node_from_substring(void)
{
	SerdNode empty = serd_node_from_substring(SERD_LITERAL, NULL, 32);
	assert(!empty.buf && !empty.n_bytes && !empty.n_chars && !empty.flags &&
	       !empty.type);

	SerdNode a_b = serd_node_from_substring(SERD_LITERAL, USTR("a\"bc"), 3);
	assert(a_b.n_bytes == 3 && a_b.n_chars == 3 &&
	       a_b.flags == SERD_HAS_QUOTE &&
	       !strncmp((const char*)a_b.buf, "a\"b", 3));

	a_b = serd_node_from_substring(SERD_LITERAL, USTR("a\"bc"), 10);
	assert(a_b.n_bytes == 4 && a_b.n_chars == 4 &&
	       a_b.flags == SERD_HAS_QUOTE &&
	       !strncmp((const char*)a_b.buf, "a\"bc", 4));
}

int
main(void)
{
	test_string_to_double();
	test_double_to_node();
	test_integer_to_node();
	test_blob_to_node();
	test_node_equals();
	test_node_from_string();
	test_node_from_substring();

	printf("Success\n");
	return 0;
}
