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

#include "../src/decimal.h"
#include "../src/string_utils.h"
#include "test_data.h"

#include "serd/serd.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DBL_INFINITY ((double)INFINITY)

static size_t   n_tests = 16384ul;
static uint32_t seed    = 0;

static void
test_count_digits(void)
{
	assert(1 == serd_count_digits(0));
	assert(1 == serd_count_digits(1));
	assert(1 == serd_count_digits(9));
	assert(2 == serd_count_digits(10));
	assert(2 == serd_count_digits(99ull));
	assert(3 == serd_count_digits(999ull));
	assert(4 == serd_count_digits(9999ull));
	assert(5 == serd_count_digits(99999ull));
	assert(6 == serd_count_digits(999999ull));
	assert(7 == serd_count_digits(9999999ull));
	assert(8 == serd_count_digits(99999999ull));
	assert(9 == serd_count_digits(999999999ull));
	assert(10 == serd_count_digits(9999999999ull));
	assert(11 == serd_count_digits(99999999999ull));
	assert(12 == serd_count_digits(999999999999ull));
	assert(13 == serd_count_digits(9999999999999ull));
	assert(14 == serd_count_digits(99999999999999ull));
	assert(15 == serd_count_digits(999999999999999ull));
	assert(16 == serd_count_digits(9999999999999999ull));
	assert(17 == serd_count_digits(99999999999999999ull));
	assert(18 == serd_count_digits(999999999999999999ull));
	assert(19 == serd_count_digits(9999999999999999999ull));
	assert(20 == serd_count_digits(18446744073709551615ull));
}

static void
test_strtod(void)
{
	assert(serd_strtod("1E999", NULL) == (double)INFINITY);
	assert(serd_strtod("-1E999", NULL) == (double)-INFINITY);
	assert(serd_strtod("1E-999", NULL) == 0.0);
	assert(serd_strtod("-1E-999", NULL) == -0.0);
	assert(isnan(serd_strtod("ABCDEF", NULL)));
}

static void
check_precision(const double   d,
                const unsigned precision,
                const unsigned frac_digits,
                const char*    expected)
{
	SerdNode* const node = serd_new_decimal(d, precision, frac_digits, NULL);
	const char*     str  = serd_node_get_string(node);

	if (strcmp(str, expected)) {
		fprintf(stderr, "error: string is \"%s\"\n", str);
		fprintf(stderr, "note:  expected  \"%s\"\n", expected);
		assert(false);
	}

	serd_node_free(node);
}

static void
test_precision(void)
{
	assert(serd_new_decimal((double)INFINITY, 17, 0, NULL) == NULL);
	assert(serd_new_decimal((double)-INFINITY, 17, 0, NULL) == NULL);
	assert(serd_new_decimal((double)NAN, 17, 0, NULL) == NULL);

	check_precision(1.0000000001, 17, 8, "1.0");
	check_precision(0.0000000001, 17, 10, "0.0000000001");
	check_precision(0.0000000001, 17, 8, "0.0");

	check_precision(12345.678900, 9, 5, "12345.6789");
	check_precision(12345.678900, 8, 5, "12345.678");
	check_precision(12345.678900, 5, 5, "12345.0");
	check_precision(12345.678900, 3, 5, "12300.0");

	check_precision(12345.678900, 9, 0, "12345.6789");
	check_precision(12345.678900, 9, 5, "12345.6789");
	check_precision(12345.678900, 9, 3, "12345.678");
	check_precision(12345.678900, 9, 1, "12345.6");
}

/// Check that `str` is a canonical xsd:float or xsd:double string
static void
test_canonical(const char* str, const size_t len)
{
	if (!strcmp(str, "NaN") || !strcmp(str, "-INF") || !strcmp(str, "INF")) {
		return;
	}

	assert(len > 4); // Shortest possible is something like 1.2E3
	assert(str[0] == '-' || is_digit(str[0]));

	const int first_digit = str[0] == '-' ? 1 : 0;
	assert(is_digit(str[first_digit]));
	assert(str[first_digit + 1] == '.');
	assert(is_digit(str[first_digit + 2]));

	const char* const e = strchr(str, 'E');
	assert(e);
	assert(*e == 'E');
	assert(*(e + 1) == '-' || is_digit(*(e + 1)));
}

/// Check that `f` round-trips, and serialises to `expected` if given
static void
test_float_value(const float f, const char* expected)
{
	SerdNode* const node   = serd_new_float(f);
	const char*     str    = serd_node_get_string(node);
	size_t          end    = 0;
	const float     result = (float)serd_strtod(str, &end);
	const bool      match  = result == f || (isnan(f) && isnan(result));

	if (!match) {
		fprintf(stderr, "error: value is %.9g\n", (double)result);
		fprintf(stderr, "note:  expected %.9g\n", (double)f);
		fprintf(stderr, "note:  string   %s\n", str);
	}

	assert(match);
	assert(end == serd_node_get_length(node));
	assert((isnan(f) && isnan(result)) || result == f);
	assert(!expected || !strcmp(str, expected));

	test_canonical(str, serd_node_get_length(node));
	serd_node_free(node);
}

static void
test_float(const bool exhaustive)
{
	test_float_value(NAN, "NaN");
	test_float_value(-INFINITY, "-INF");
	test_float_value(INFINITY, "INF");

	test_float_value(-0.0f, "-0.0E0");
	test_float_value(+0.0f, "0.0E0");
	test_float_value(-1.0f, "-1.0E0");
	test_float_value(+1.0f, "1.0E0");

	test_float_value(5.0f, "5.0E0");
	test_float_value(50.0f, "5.0E1");
	test_float_value(5000000000.0f, "5.0E9");
	test_float_value(-0.5f, "-5.0E-1");
	test_float_value(0.5f, "5.0E-1");
	test_float_value(0.0625f, "6.25E-2");
	test_float_value(0.0078125f, "7.8125E-3");

	// Every digit of precision
	test_float_value(134217728.0f, "1.34217728E8");

	// Normal limits
	test_float_value(FLT_MIN, NULL);
	test_float_value(FLT_EPSILON, NULL);
	test_float_value(FLT_MAX, NULL);

	// Subnormals
	test_float_value(nextafterf(0.0f, 1.0f), NULL);
	test_float_value(nextafterf(0.0f, -1.0f), NULL);

	// Past limits
	assert((float)serd_strtod("1e39", NULL) == INFINITY);
	assert((float)serd_strtod("1e-46", NULL) == 0.0f);

	// Powers of two (where the lower boundary is closer)
	for (int i = -127; i <= 127; ++i) {
		test_float_value(powf(2, (float)i), NULL);
	}

	if (exhaustive) {
		fprintf(stderr, "Testing xsd:float exhaustively\n");

		for (uint64_t i = 0; i <= UINT32_MAX; ++i) {
			const float f = float_from_rep((uint32_t)i);
			test_float_value(f, NULL);

			if (i % 1000000 == 1) {
				fprintf(stderr, "%f%%\n", (i / (double)UINT32_MAX * 100.0));
			}
		}
	} else {
		fprintf(stderr, "Testing xsd:float randomly\n");
		const size_t n_per_report = n_tests / 10ul;
		uint64_t     last_report  = 0;
		uint32_t     rep          = seed;
		for (uint64_t i = 0; i < n_tests; ++i) {
			rep = lcg32(rep);

			const float f = float_from_rep(rep);

			test_float_value(nextafterf(f, -INFINITY), NULL);
			test_float_value(f, NULL);
			test_float_value(nextafterf(f, INFINITY), NULL);

			if (i / n_per_report != last_report / n_per_report) {
				fprintf(stderr,
				        "%u%%\n",
				        (unsigned)(i / (double)n_tests * 100.0));
				last_report = i;
			}
		}
	}
}

/// Check that `d` round-trips, and serialises to `expected` if given
static void
test_double_value(const double d, const char* expected)
{
	SerdNode* const node   = serd_new_double(d);
	const char*     str    = serd_node_get_string(node);
	size_t          end    = 0;
	const double    result = serd_strtod(str, &end);
	const bool      match  = result == d || (isnan(d) && isnan(result));

	if (!match) {
		fprintf(stderr, "error: value is %.17g\n", result);
		fprintf(stderr, "note:  expected %.17g\n", d);
		fprintf(stderr, "note:  string   %s\n", str);
	}

	assert(match);
	assert(end == serd_node_get_length(node));
	assert((isnan(d) && isnan(result)) || result == d);
	assert(!expected || !strcmp(str, expected));

	test_canonical(str, serd_node_get_length(node));
	serd_node_free(node);
}

static void
test_double(void)
{
	test_double_value((double)NAN, "NaN");
	test_double_value(-DBL_INFINITY, "-INF");
	test_double_value(DBL_INFINITY, "INF");

	test_double_value(-0.0, "-0.0E0");
	test_double_value(+0.0, "0.0E0");
	test_double_value(-1.0, "-1.0E0");
	test_double_value(+1.0, "1.0E0");

	test_double_value(5.0, "5.0E0");
	test_double_value(50.0, "5.0E1");
	test_double_value(500000000000000000000.0, "5.0E20");
	test_double_value(-0.5, "-5.0E-1");
	test_double_value(0.5, "5.0E-1");
	test_double_value(0.05, "5.0E-2");
	test_double_value(0.005, "5.0E-3");
	test_double_value(0.00000000000000000005, "5.0E-20");

	// Leading whitespace special cases
	assert(isnan(serd_strtod(" NaN", NULL)));
	assert(serd_strtod(" -INF", NULL) == -DBL_INFINITY);
	assert(serd_strtod(" INF", NULL) == DBL_INFINITY);
	assert(serd_strtod(" +INF", NULL) == DBL_INFINITY);

	// Every digit of precision
	test_double_value(18014398509481984.0, "1.8014398509481984E16");

	// Normal limits
	test_double_value(DBL_MIN, NULL);
	test_double_value(nextafter(DBL_MIN, DBL_INFINITY), NULL);
	test_double_value(DBL_EPSILON, NULL);
	test_double_value(DBL_MAX, NULL);
	test_double_value(nextafter(DBL_MAX, -DBL_INFINITY), NULL);

	// Subnormals
	test_double_value(nextafter(0.0, 1.0), NULL);
	test_double_value(nextafter(nextafter(0.0, 1.0), 1.0), NULL);
	test_double_value(nextafter(0.0, -1.0), NULL);
	test_double_value(nextafter(nextafter(0.0, -1.0), -1.0), NULL);

	// Past limits
	assert(serd_strtod("1e309", NULL) == DBL_INFINITY);
	assert(serd_strtod("12345678901234567123", NULL) == 12345678901234567000.0);
	assert(serd_strtod("1e-325", NULL) == 0.0);

	// Various tricky cases
	test_double_value(1e23, "1.0E23");
	test_double_value(6.02951420360127e-309, "6.02951420360127E-309");
	test_double_value(9.17857104364115e+288, "9.17857104364115E288");
	test_double_value(2.68248422823759e+22, "2.68248422823759E22");

	// Powers of two (where the lower boundary is closer)
	for (int i = -1023; i <= 1023; ++i) {
		test_double_value(pow(2, i), NULL);
	}

	fprintf(stderr, "Testing xsd:double randomly\n");

	const size_t n_per_report = n_tests / 10ul;
	uint64_t     last_report  = 0;
	uint64_t     rep          = seed;
	for (uint64_t i = 0; i < n_tests; ++i) {
		rep = lcg64(rep);

		const double d = double_from_rep(rep);

		test_double_value(nextafter(d, -DBL_INFINITY), NULL);
		test_double_value(d, NULL);
		test_double_value(nextafter(d, DBL_INFINITY), NULL);

		if (i / n_per_report != last_report / n_per_report) {
			fprintf(stderr, "%u%%\n", (unsigned)(i / (double)n_tests * 100.0));
			last_report = i;
		}
	}
}

/// Check that `d` round-trips, and serialises to `expected` if given
static void
test_decimal_value(const double d, const char* expected)
{
	SerdNode* const node   = serd_new_decimal(d, 17, 0, NULL);
	const char*     str    = serd_node_get_string(node);
	size_t          end    = 0;
	const double    result = serd_strtod(str, &end);
	const bool      match  = result == d || (isnan(d) && isnan(result));

	if (!match) {
		fprintf(stderr, "error: value is %.17g\n", result);
		fprintf(stderr, "note:  expected %.17g\n", d);
		fprintf(stderr, "note:  string   %s\n", str);
	}

	assert(match);
	assert(end == serd_node_get_length(node));

	if (expected && strcmp(str, expected)) {
		fprintf(stderr, "error: string is \"%s\"\n", str);
		fprintf(stderr, "note:  expected  \"%s\"\n", expected);
	}
	assert(!expected || !strcmp(str, expected));

	serd_node_free(node);
}

static void
test_decimal(void)
{
	test_decimal_value(-0.0, "-0.0");
	test_decimal_value(+0.0, "0.0");
	test_decimal_value(-1.0, "-1.0");
	test_decimal_value(+1.0, "1.0");

	test_decimal_value(5.0, "5.0");
	test_decimal_value(50.0, "50.0");
	test_decimal_value(500000000000000000000.0, "500000000000000000000.0");
	test_decimal_value(-0.5, "-0.5");
	test_decimal_value(0.5, "0.5");
	test_decimal_value(0.05, "0.05");
	test_decimal_value(0.005, "0.005");
	test_decimal_value(0.00000000000000000005, "0.00000000000000000005");

	// Every digit of precision
	test_decimal_value(18014398509481984.0, "18014398509481984.0");

	// Normal limits
	test_decimal_value(DBL_MIN, NULL);
	test_decimal_value(nextafter(DBL_MIN, DBL_INFINITY), NULL);
	test_decimal_value(DBL_EPSILON, NULL);
	test_decimal_value(DBL_MAX, NULL);
	test_decimal_value(nextafter(DBL_MAX, -DBL_INFINITY), NULL);

	// Subnormals
	test_decimal_value(nextafter(0.0, 1.0), NULL);
	test_decimal_value(nextafter(nextafter(0.0, 1.0), 1.0), NULL);
	test_decimal_value(nextafter(0.0, -1.0), NULL);
	test_decimal_value(nextafter(nextafter(0.0, -1.0), -1.0), NULL);

	// Past limits
	assert(serd_strtod("1e309", NULL) == DBL_INFINITY);
	assert(serd_strtod("12345678901234567123", NULL) == 12345678901234567000.0);
	assert(serd_strtod("1e-325", NULL) == 0.0);

	// Various tricky cases
	test_decimal_value(1e23, NULL);
	test_decimal_value(6.02951420360127e-309, NULL);
	test_decimal_value(9.17857104364115e+288, NULL);
	test_decimal_value(2.68248422823759e+22, NULL);

	// Powers of two (where the lower boundary is closer)
	for (int i = -1023; i <= 1023; ++i) {
		test_decimal_value(pow(2, i), NULL);
	}

	fprintf(stderr, "Testing xsd:decimal randomly\n");

	const size_t n_per_report = n_tests / 10ul;
	uint64_t     last_report  = 0;
	uint64_t     rep          = seed;
	for (uint64_t i = 0; i < n_tests; ++i) {
		rep = lcg64(rep);

		const double d = double_from_rep(rep);
		if (!isfinite(d)) {
			continue;
		}

		test_decimal_value(nextafter(d, (double)-INFINITY), NULL);
		test_decimal_value(d, NULL);
		test_decimal_value(nextafter(d, (double)INFINITY), NULL);

		if (i / n_per_report != last_report / n_per_report) {
			fprintf(stderr, "%u%%\n", (unsigned)(i / (double)n_tests * 100.0));
			last_report = i;
		}
	}
}

static int
print_usage(const char* name)
{
	fprintf(stderr, "Usage: %s [OPTION]...\n", name);
	fprintf(stderr, "Test floating point conversion.\n");
	fprintf(stderr, "  -n NUM_TESTS Number of random tests to run.\n");
	fprintf(stderr, "  -s SEED      Use random seed.\n");
	fprintf(stderr, "  -x           Exhaustively test floats.\n");
	return 1;
}

int
main(int argc, char** argv)
{
	// Parse command line arguments
	int  a          = 1;
	bool exhaustive = false;
	for (; a < argc && argv[a][0] == '-'; ++a) {
		if (argv[a][1] == '\0') {
			break;
		} else if (argv[a][1] == 'x') {
			exhaustive = true;
		} else if (argv[a][1] == 's') {
			if (++a == argc) {
				return print_usage(argv[0]);
			}

			seed = (uint32_t)strtol(argv[a], NULL, 10);
		} else if (argv[a][1] == 'n') {
			if (++a == argc) {
				return print_usage(argv[0]);
			}

			n_tests = (uint32_t)strtol(argv[a], NULL, 10);
		}
	}

	if (!seed) {
		seed = (uint32_t)time(NULL) + (uint32_t)getpid();
	}

	fprintf(stderr, "Using random seed %u\n", seed);

	test_count_digits();
	test_strtod();
	test_precision();
	test_float(exhaustive);
	test_double();
	test_decimal();

	fprintf(stderr, "All tests passed\n");
	return 0;
}
