/*
  Copyright 2011 David Robillard <http://drobilla.net>

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

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"

static bool
test_strtod(double dbl, double max_delta)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%lf", dbl);

	char* endptr = NULL;
	const double out = serd_strtod(buf, &endptr);

	const double diff = fabs(out - dbl);
	if (diff > max_delta) {
		fprintf(stderr, "error: Parsed %lf != %lf (delta %lf)\n",
		        dbl, out, diff);
		return false;
	}
	return true;
}

int
main()
{
	#define MAX       1000000
	#define NUM_TESTS 1000
	for (int i = 0; i < NUM_TESTS; ++i) {
		double dbl = rand() % MAX;
		dbl += (rand() % MAX) / (double)MAX;

		if (!test_strtod(dbl, 1 / (double)MAX)) {
			return 1;
		}
	}

	const double expt_test_nums[] = {
		2.0E18, -5e19, +8e20, 2e+34, -5e-5, 8e0, 9e-0, 2e+0
	};

	const char* expt_test_strs[] = {
		"02e18", "-5e019", "+8e20", "2E+34", "-5E-5", "8E0", "9e-0", "2e+0"
	};

	for (unsigned i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
		char* endptr;
		const double num   = serd_strtod(expt_test_strs[i], &endptr);
		const double delta = fabs(num - expt_test_nums[i]);
		if (delta > DBL_EPSILON) {
			fprintf(stderr, "error: Parsed `%s' %lf != %lf (delta %lf)\n",
			        expt_test_strs[i], num, expt_test_nums[i], delta);
			return 1;
		}
	}

	// Test serd_node_new_decimal

	const double dbl_test_nums[] = {
		0.0, 42.0, .01, 8.0, 2.05, -16.00001, 5.000000005
	};

	const char* dbl_test_strs[] = {
		"0.0", "42.0", "0.01", "8.0", "2.05", "-16.00001", "5.00000001"
	};

	for (unsigned i = 0; i < sizeof(dbl_test_nums) / sizeof(double); ++i) {
		SerdNode node = serd_node_new_decimal(dbl_test_nums[i], 8);
		if (strcmp((const char*)node.buf, (const char*)dbl_test_strs[i])) {
			fprintf(stderr, "error: Serialised `%s' != %s\n",
			        node.buf, dbl_test_strs[i]);
			return 1;
		}
		const size_t len = strlen((const char*)node.buf);
		if (node.n_bytes != len || node.n_chars != len) {
			fprintf(stderr, "error: Length %zu,%zu != %zu\n",
			        node.n_bytes, node.n_chars, len);
			return 1;
		}
		serd_node_free(&node);
	}

	// Test serd_node_new_integer

	const long int_test_nums[] = {
		0, -0, -23, 23, -12340, 1000, -1000
	};

	const char* int_test_strs[] = {
		"0", "0", "-23", "23", "-12340", "1000", "-1000"
	};

	for (unsigned i = 0; i < sizeof(int_test_nums) / sizeof(double); ++i) {
		fprintf(stderr, "\n*** TEST %ld\n", int_test_nums[i]);
		SerdNode node = serd_node_new_integer(int_test_nums[i]);
		if (strcmp((const char*)node.buf, (const char*)int_test_strs[i])) {
			fprintf(stderr, "error: Serialised `%s' != %s\n",
			        node.buf, int_test_strs[i]);
			return 1;
		}
		const size_t len = strlen((const char*)node.buf);
		if (node.n_bytes != len || node.n_chars != len) {
			fprintf(stderr, "error: Length %zu,%zu != %zu\n",
			        node.n_bytes, node.n_chars, len);
			return 1;
		}
		serd_node_free(&node);
	}

	// Test serd_strlen
	const uint8_t str[] = { '"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0 };

	size_t        n_bytes;
	SerdNodeFlags flags;
	const size_t  len = serd_strlen(str, &n_bytes, &flags);
	if (len != 5 || n_bytes != 7
	    || flags != (SERD_HAS_QUOTE|SERD_HAS_NEWLINE)) {
		fprintf(stderr, "Bad serd_strlen(%s) len=%zu n_bytes=%zu flags=%u\n",
		        str, len, n_bytes, flags);
		return 1;
	}

	// Test serd_strerror
	const uint8_t* msg = NULL;
	if (strcmp((const char*)(msg = serd_strerror(SERD_SUCCESS)), "Success")) {
		fprintf(stderr, "Bad message `%s' for SERD_SUCCESS\n", msg);
		return 1;
	}
	for (int i = SERD_FAILURE; i <= SERD_ERR_NOT_FOUND; ++i) {
		msg = serd_strerror((SerdStatus)i);
		if (!strcmp((const char*)msg, "Success")) {
			fprintf(stderr, "Bad message `%s' for (SerdStatus)%d\n", msg, i);
			return 1;
		}
	}
	
	printf("Success\n");
	return 0;
}
