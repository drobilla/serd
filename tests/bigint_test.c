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

#include "../src/bigint.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_HEX_LEN 512

/* Some test data borrowed from http://github.com/google/double-conversion
   which uses a completely different bigint representation (so if these agree,
   everything is probably fine).  Others cases are either made up to hit the
   edges of the implementation, or interesting cases collected from testing the
   decimal implementation.  Almost everything here uses the hex representation
   so it is easy to dump into Python as a sanity check. */

static inline SerdBigint
bigint_from_hex(const char* str)
{
	SerdBigint num;
	serd_bigint_set_hex_string(&num, str);
	return num;
}

static bool
check_string_equals(const char* const actual, const char* const expected)
{
	if (strcmp(actual, expected)) {
		fprintf(stderr, "error: result   \"%s\"\n", actual);
		fprintf(stderr, "note:  expected \"%s\"\n", expected);
		return false;
	}

	return true;
}

static bool
check_hex_equals(const char* const str, const SerdBigint* const num)
{
	char buffer[MAX_HEX_LEN] = {0};
	serd_bigint_to_hex_string(num, buffer, sizeof(buffer));

	const char* s = str;
	while (*s == ' ') {
		++s; // Skip leading whitespace
	}

	return check_string_equals(buffer, s);
}

#define CHECK_HEXEQ(str, num) assert(check_hex_equals(str, num))
#define CHECK_STREQ(s1, s2) assert(check_string_equals(s1, s2))

#define CHECK_SET(setter, value, expected)     \
	do {                                       \
		SerdBigint num;                        \
		serd_bigint_set_##setter(&num, value); \
		CHECK_HEXEQ(expected, &num);           \
	} while (0)

static void
test_set(void)
{
	CHECK_SET(u32, 0, "0");
	CHECK_SET(u32, 0xA, "A");
	CHECK_SET(u32, 0x20, "20");
	CHECK_SET(u32, 0x12345678, "12345678");

	CHECK_SET(u64, 0, "0");
	CHECK_SET(u64, 0xA, "A");
	CHECK_SET(u64, 0x20, "20");
	CHECK_SET(u64, 0x12345678, "12345678");
	CHECK_SET(u64, 0xFFFFFFFFFFFFFFFFull, "FFFFFFFFFFFFFFFF");
	CHECK_SET(u64, 0x123456789ABCDEF0ull, "123456789ABCDEF0");
	CHECK_SET(u64, 0x123456789ABCDEF0ull, "123456789ABCDEF0");

	CHECK_SET(decimal_string, "0", "0");
	CHECK_SET(decimal_string, "1", "1");
	CHECK_SET(decimal_string, "01234567890", "499602D2");
	CHECK_SET(decimal_string, "12345.67890", "499602D2");
	CHECK_SET(decimal_string, "12345.67890EOF", "499602D2");
	CHECK_SET(decimal_string, "012345678901", "2DFDC1C35");
	CHECK_SET(decimal_string, "12345.678901", "2DFDC1C35");
	CHECK_SET(decimal_string,
	          "340282366920938463463374607431768211456",
	          "100000000000000000000000000000000");

	CHECK_SET(hex_string, "0", "0");
	CHECK_SET(hex_string, "123456789ABCDEF0", "123456789ABCDEF0");

	const SerdBigint orig = bigint_from_hex("123456789ABCDEF01");
	SerdBigint       copy;
	serd_bigint_set(&copy, &orig);
	CHECK_HEXEQ("123456789ABCDEF01", &copy);
}

static void
check_print_hex(const char* value, const size_t len)
{
	assert(len <= MAX_HEX_LEN);

	FILE* const      stream = tmpfile();
	const SerdBigint num    = bigint_from_hex(value);

	serd_bigint_print_hex(stream, &num);
	fseek(stream, 0, SEEK_SET);

	char         buf[MAX_HEX_LEN] = {0};
	const size_t n_read           = fread(buf, 1, len, stream);

	fclose(stream);

	CHECK_STREQ(buf, value);
	assert(n_read == len);
}

static void
check_to_hex_string(const char*  value,
                    const size_t len,
                    const char*  expected,
                    const size_t expected_n_written)
{
	const SerdBigint num              = bigint_from_hex(value);
	char             buf[MAX_HEX_LEN] = {0};
	const size_t     n_written = serd_bigint_to_hex_string(&num, buf, len);

	assert(n_written == expected_n_written);
	CHECK_STREQ(buf, expected);
}

static void
test_output(void)
{
	check_print_hex("0", 1);
	check_print_hex("1", 1);
	check_print_hex("1234567", 7);
	check_print_hex("12345678", 8);
	check_print_hex("123456789ABCDEF0", 16);
	check_print_hex("123456789ABCDEF01", 17);

	check_to_hex_string("123456789", 1, "", 0);
	check_to_hex_string("123456789", 9, "12345678", 9);
	check_to_hex_string("123456789", 10, "123456789", 9);
	check_to_hex_string("123456789ABCDEF", 16, "123456789ABCDEF", 15);
}

static void
check_left_shifted_bigit(const char*    value,
                         const unsigned amount,
                         const unsigned index,
                         const Bigit    expected)
{
	const SerdBigint num = bigint_from_hex(value);
	const Bigit actual   = serd_bigint_left_shifted_bigit(&num, amount, index);

	assert(expected == actual);
}

static void
test_left_shifted_bigit(void)
{
	check_left_shifted_bigit("0", 100, 1, 0x0);
	check_left_shifted_bigit("1", 0, 0, 0x1);
	check_left_shifted_bigit("1", 1, 0, 0x2);
	check_left_shifted_bigit("1", 4, 0, 0x10);
	check_left_shifted_bigit("1", 32, 0, 0x0);
	check_left_shifted_bigit("1", 32, 1, 0x1);
	check_left_shifted_bigit("1", 64, 0, 0x0);
	check_left_shifted_bigit("1", 64, 1, 0x0);
	check_left_shifted_bigit("1", 64, 2, 0x1);
	check_left_shifted_bigit("123456789ABCDEF", 64, 0, 0x0);
	check_left_shifted_bigit("123456789ABCDEF", 64, 1, 0x0);
	check_left_shifted_bigit("123456789ABCDEF", 64, 2, 0x89ABCDEF);
	check_left_shifted_bigit("123456789ABCDEF", 64, 3, 0x1234567);
	check_left_shifted_bigit("123456789ABCDEF", 64, 4, 0x0);
	check_left_shifted_bigit("123456789ABCDEF", 65, 0, 0x0);
	check_left_shifted_bigit("123456789ABCDEF", 65, 1, 0x0);
	check_left_shifted_bigit("123456789ABCDEF", 65, 2, 0x13579BDE);
	check_left_shifted_bigit("123456789ABCDEF", 65, 3, 0x2468ACF);
	check_left_shifted_bigit("123456789ABCDEF", 65, 4, 0x0);
}

static void
check_shift_left(const char* value, const unsigned amount, const char* expected)
{
	SerdBigint num = bigint_from_hex(value);
	serd_bigint_shift_left(&num, amount);
	CHECK_HEXEQ(expected, &num);
}

static void
test_shift_left(void)
{
	check_shift_left("0", 100, "0");
	check_shift_left("1", 1, "2");
	check_shift_left("1", 4, "10");
	check_shift_left("1", 32, "100000000");
	check_shift_left("1", 64, "10000000000000000");
	check_shift_left("123456789ABCDEF", 0, "123456789ABCDEF");
	check_shift_left("123456789ABCDEF", 64, "123456789ABCDEF0000000000000000");
	check_shift_left("123456789ABCDEF", 65, "2468ACF13579BDE0000000000000000");
	check_shift_left("16B8B5E06EDC79", 23, "B5C5AF0376E3C800000");
}

static void
check_add_u32(const char* value, const uint32_t rhs, const char* expected)
{
	SerdBigint num = bigint_from_hex(value);
	serd_bigint_add_u32(&num, rhs);
	CHECK_HEXEQ(expected, &num);
}

static void
test_add_u32(void)
{
	check_add_u32("0", 1, "1");
	check_add_u32("1", 1, "2");
	check_add_u32("FFFFFFF", 1, "10000000");
	check_add_u32("FFFFFFFFFFFFFF", 1, "100000000000000");

	check_add_u32("10000000000000000000000000000000000080000000",
	              0x80000000,
	              "10000000000000000000000000000000000100000000");

	check_add_u32("10000000000000000000000000000000000000000000",
	              0x1,
	              "10000000000000000000000000000000000000000001");
}

static void
check_add(const char* lhs_hex, const char* rhs_hex, const char* expected)
{
	SerdBigint       lhs = bigint_from_hex(lhs_hex);
	const SerdBigint rhs = bigint_from_hex(rhs_hex);

	serd_bigint_add(&lhs, &rhs);
	CHECK_HEXEQ(expected, &lhs);
}

static void
test_add(void)
{
	check_add("1", "0", "1");
	check_add("1", "1", "2");
	check_add("FFFFFFF", "1", "10000000");
	check_add("FFFFFFFFFFFFFF", "1", "100000000000000");
	check_add("1", "1000000000000", "1000000000001");
	check_add("FFFFFFF", "1000000000000", "100000FFFFFFF");

	check_add("10000000000000000000000000000000000000000000",
	          "1",
	          "10000000000000000000000000000000000000000001");

	check_add("10000000000000000000000000000000000000000000",
	          "1000000000000",
	          "10000000000000000000000000000001000000000000");

	check_add("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
	          "1000000000000",
	          "1000000000000000000000000000000FFFFFFFFFFFF");

	check_add("10000000000000000000000000",
	          "1000000000000",
	          "10000000000001000000000000");

	check_add("1",
	          "10000000000000000000000000000",
	          "10000000000000000000000000001");

	check_add("FFFFFFF",
	          "10000000000000000000000000000",
	          "1000000000000000000000FFFFFFF");

	check_add("10000000000000000000000000000000000000000000",
	          "10000000000000000000000000000",
	          "10000000000000010000000000000000000000000000");

	check_add("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
	          "10000000000000000000000000000",
	          "100000000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFF");

	check_add("10000000000000000000000000",
	          "10000000000000000000000000000",
	          "10010000000000000000000000000");
}

static void
check_subtract(const char* lhs_hex, const char* rhs_hex, const char* expected)
{
	SerdBigint       lhs = bigint_from_hex(lhs_hex);
	const SerdBigint rhs = bigint_from_hex(rhs_hex);

	serd_bigint_subtract(&lhs, &rhs);
	CHECK_HEXEQ(expected, &lhs);
}

static void
test_subtract(void)
{
	check_subtract("1", "0", "1");
	check_subtract("2", "0", "2");
	check_subtract("10000000", "1", "FFFFFFF");
	check_subtract("1FFFFFFFF00000000", "FFFFFFFF", "1FFFFFFFE00000001");
	check_subtract("100000000000000", "1", "FFFFFFFFFFFFFF");
	check_subtract("1000000000001", "1000000000000", "1");
	check_subtract("100000FFFFFFF", "1000000000000", "FFFFFFF");

	check_subtract("11F2678326EA00000000",
	               "0878678326EAC9000000",
	               "979FFFFFFFF37000000");

	check_subtract("10000000000000000000000000000000000000000001",
	               "00000000000000000000000000000000000000000001",
	               "10000000000000000000000000000000000000000000");

	check_subtract("10000000000000000000000000000001000000000000",
	               "00000000000000000000000000000001000000000000",
	               "10000000000000000000000000000000000000000000");

	check_subtract("1000000000000000000000000000000FFFFFFFFFFFF",
	               "0000000000000000000000000000001000000000000",
	               " FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

	check_subtract("10000000000000000000000000",
	               "00000000000001000000000000",
	               " FFFFFFFFFFFFF000000000000");

	check_subtract("10000000000000000000000000",
	               "1000000000000000000000000",
	               "F000000000000000000000000");

	check_subtract("FFFFFFF000000000000000",
	               "0000000000000800000000",
	               "FFFFFFEFFFFFF800000000");

	check_subtract("10000000000000000000000000000000000000000000",
	               "00000000000000000000000000000000000800000000",
	               "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF800000000");

	check_subtract("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
	               "000000000000000000000000000000000800000000",
	               "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFF");
}

static void
check_subtract_left_shifted(const char*    lhs_hex,
                            const char*    rhs_hex,
                            const unsigned amount,
                            const char*    expected)
{
	SerdBigint       lhs = bigint_from_hex(lhs_hex);
	const SerdBigint rhs = bigint_from_hex(rhs_hex);

	serd_bigint_subtract_left_shifted(&lhs, &rhs, amount);
	CHECK_HEXEQ(expected, &lhs);
}

static void
test_subtract_left_shifted(void)
{
	check_subtract_left_shifted("1", "0", 1, "1");
	check_subtract_left_shifted("10000000", "1", 1, "FFFFFFE");
	check_subtract_left_shifted("100000000", "40000000", 2, "0");
	check_subtract_left_shifted("1000000000000000", "400000000000000", 2, "0");
	check_subtract_left_shifted("1000000000000000", "800000000000000", 1, "0");
	check_subtract_left_shifted("1000000000000000", "F", 16, "FFFFFFFFFF10000");
	check_subtract_left_shifted("1000000000000000", "F", 24, "FFFFFFFF1000000");
	check_subtract_left_shifted("100000000000000", "1", 0, "FFFFFFFFFFFFFF");
	check_subtract_left_shifted("100000000000000", "1", 56, "0");

	check_subtract_left_shifted("11F2678326EA00000000",
	                            "43C33C1937564800000",
	                            1,
	                            "979FFFFFFFF37000000");
}

static void
check_multiply_u32(const char* value, const uint32_t rhs, const char* expected)
{
	SerdBigint num = bigint_from_hex(value);
	serd_bigint_multiply_u32(&num, rhs);
	CHECK_HEXEQ(expected, &num);
}

static void
test_multiply_u32(void)
{
	check_multiply_u32("0", 0x25, "0");
	check_multiply_u32("123456789ABCDEF", 0, "0");
	check_multiply_u32("2", 0x5, "A");
	check_multiply_u32("10000000", 0x9, "90000000");
	check_multiply_u32("100000000000000", 0xFFFF, "FFFF00000000000000");
	check_multiply_u32("100000000000000", 0xFFFFFFFF, "FFFFFFFF00000000000000");
	check_multiply_u32("1234567ABCD", 0xFFF, "12333335552433");
	check_multiply_u32("1234567ABCD", 0xFFFFFFF, "12345679998A985433");
	check_multiply_u32("FFFFFFFFFFFFFFFF", 0x2, "1FFFFFFFFFFFFFFFE");
	check_multiply_u32("FFFFFFFFFFFFFFFF", 0x4, "3FFFFFFFFFFFFFFFC");
	check_multiply_u32("FFFFFFFFFFFFFFFF", 0xF, "EFFFFFFFFFFFFFFF1");
	check_multiply_u32("FFFFFFFFFFFFFFFF", 0xFFFFFF, "FFFFFEFFFFFFFFFF000001");
	check_multiply_u32("377654D193A171", 10000000, "210EDD6D4CDD2580EE80");
	check_multiply_u32("2E3F36D108373C00000", 10, "1CE78242A52285800000");

	check_multiply_u32("10000000000000000000000000",
	                   0x00000002,
	                   "20000000000000000000000000");

	check_multiply_u32("10000000000000000000000000",
	                   0x0000000F,
	                   "F0000000000000000000000000");

	check_multiply_u32("FFFF0000000000000000000000000",
	                   0xFFFF,
	                   "FFFE00010000000000000000000000000");

	check_multiply_u32("FFFF0000000000000000000000000",
	                   0xFFFFFFFF,
	                   "FFFEFFFF00010000000000000000000000000");

	check_multiply_u32("FFFF0000000000000000000000000",
	                   0xFFFFFFFF,
	                   "FFFEFFFF00010000000000000000000000000");
}

static void
check_multiply_u64(const char* value, const uint64_t rhs, const char* expected)
{
	SerdBigint num = bigint_from_hex(value);
	serd_bigint_multiply_u64(&num, rhs);
	CHECK_HEXEQ(expected, &num);
}

static void
test_multiply_u64(void)
{
	check_multiply_u64("0", 0x25, "0");
	check_multiply_u64("123456789ABCDEF", 0, "0");
	check_multiply_u64("123456789ABCDEF", 1, "123456789ABCDEF");
	check_multiply_u64("2", 0x5, "A");
	check_multiply_u64("10000000", 0x9, "90000000");
	check_multiply_u64("100000000000000", 0xFFFF, "FFFF00000000000000");
	check_multiply_u64("1234567ABCD", 0xFFF, "12333335552433");
	check_multiply_u64("1234567ABCD", 0xFFFFFFFFFFull, "1234567ABCBDCBA985433");
	check_multiply_u64("FFFFFFFFFFFFFFFF", 0x2, "1FFFFFFFFFFFFFFFE");
	check_multiply_u64("FFFFFFFFFFFFFFFF", 0x4, "3FFFFFFFFFFFFFFFC");
	check_multiply_u64("FFFFFFFFFFFFFFFF", 0xF, "EFFFFFFFFFFFFFFF1");

	check_multiply_u64("100000000000000",
	                   0xFFFFFFFFFFFFFFFFull,
	                   "FFFFFFFFFFFFFFFF00000000000000");

	check_multiply_u64("FFFFFFFFFFFFFFFF",
	                   0xFFFFFFFFFFFFFFFFull,
	                   "FFFFFFFFFFFFFFFE0000000000000001");

	check_multiply_u64("10000000000000000000000000",
	                   0x00000002,
	                   "20000000000000000000000000");

	check_multiply_u64("10000000000000000000000000",
	                   0x0000000F,
	                   "F0000000000000000000000000");

	check_multiply_u64("FFFF0000000000000000000000000",
	                   0xFFFF,
	                   "FFFE00010000000000000000000000000");

	check_multiply_u64("FFFF0000000000000000000000000",
	                   0xFFFFFFFF,
	                   "FFFEFFFF00010000000000000000000000000");

	check_multiply_u64("FFFF0000000000000000000000000",
	                   0xFFFFFFFFFFFFFFFFull,
	                   "FFFEFFFFFFFFFFFF00010000000000000000000000000");

	check_multiply_u64("377654D193A171",
	                   0x8AC7230489E80000ull,
	                   "1E10EE4B11D15A7F3DE7F3C7680000");
}

static void
check_multiply_pow10(const char*    value,
                     const unsigned exponent,
                     const char*    expected)
{
	SerdBigint num = bigint_from_hex(value);
	serd_bigint_multiply_pow10(&num, exponent);
	CHECK_HEXEQ(expected, &num);
}

static void
test_multiply_pow10(void)
{
	check_multiply_pow10("0", 10, "0");
	check_multiply_pow10("1234", 0, "1234");
	check_multiply_pow10("4D2", 1, "3034");
	check_multiply_pow10("4D2", 2, "1E208");
	check_multiply_pow10("4D2", 3, "12D450");
	check_multiply_pow10("4D2", 4, "BC4B20");
	check_multiply_pow10("4D2", 5, "75AEF40");
	check_multiply_pow10("4D2", 6, "498D5880");
	check_multiply_pow10("4D2", 7, "2DF857500");
	check_multiply_pow10("4D2", 8, "1CBB369200");
	check_multiply_pow10("4D2", 9, "11F5021B400");
	check_multiply_pow10("4D2", 10, "B3921510800");
	check_multiply_pow10("4D2", 11, "703B4D2A5000");
	check_multiply_pow10("4D2", 12, "4625103A72000");
	check_multiply_pow10("4D2", 13, "2BD72A24874000");
	check_multiply_pow10("4D2", 14, "1B667A56D488000");
	check_multiply_pow10("4D2", 15, "11200C7644D50000");
	check_multiply_pow10("4D2", 16, "AB407C9EB0520000");
	check_multiply_pow10("4D2", 17, "6B084DE32E3340000");
	check_multiply_pow10("4D2", 18, "42E530ADFCE0080000");
	check_multiply_pow10("4D2", 19, "29CF3E6CBE0C0500000");
	check_multiply_pow10("4D2", 20, "1A218703F6C783200000");
	check_multiply_pow10("4D2", 21, "1054F4627A3CB1F400000");
	check_multiply_pow10("4D2", 22, "A3518BD8C65EF38800000");
	check_multiply_pow10("4D2", 23, "6612F7677BFB5835000000");
	check_multiply_pow10("4D2", 24, "3FCBDAA0AD7D17212000000");
	check_multiply_pow10("4D2", 25, "27DF68A46C6E2E74B4000000");
	check_multiply_pow10("4D2", 26, "18EBA166C3C4DD08F08000000");
	check_multiply_pow10("4D2", 27, "F9344E03A5B0A259650000000");
	check_multiply_pow10("4D2", 28, "9BC0B0C2478E6577DF20000000");
	check_multiply_pow10("4D2", 29, "61586E796CB8FF6AEB740000000");
	check_multiply_pow10("4D2", 30, "3CD7450BE3F39FA2D32880000000");
	check_multiply_pow10("4D2", 31, "26068B276E7843C5C3F9500000000");

	check_multiply_pow10("4D2",
	                     50,
	                     "149D1B4CFED03B23AB5F4E1196EF45C0"
	                     "8000000000000");

	check_multiply_pow10("4D2",
	                     100,
	                     "5827249F27165024FBC47DFCA9359BF3"
	                     "16332D1B91ACEECF471FBAB06D9B2000"
	                     "0000000000000000000000");

	check_multiply_pow10("4D2",
	                     305,
	                     "AFBA390D657B0829339F5B98DC852A89"
	                     "682758E01829EADFD016D1528D4D548B"
	                     "80894B9ED9C2EC6A9CABB4881302A637"
	                     "9FF3058908FEAC310C52FCA009799718"
	                     "8260B0B2E2EC96E471B7892AD9B4F9F9"
	                     "A448CBF150D2E87F3934000000000000"
	                     "00000000000000000000000000000000"
	                     "00000000000000000000000000000000");

	check_multiply_pow10("123456789ABCDEF0", 0, "123456789ABCDEF0");
	check_multiply_pow10("123456789ABCDEF0",
	                     44,
	                     "51A1AD66ACE4E5C79209330F58F52DE3"
	                     "7CEFFF1F000000000000");
	check_multiply_pow10("123456789ABCDEF0",
	                     88,
	                     "16E0C6D18F4BFA7D0289B88382F56151"
	                     "EB9DA5DB09D56C9BA5D8305619CEE057"
	                     "4F00000000000000000000000");
	check_multiply_pow10("123456789ABCDEF0",
	                     132,
	                     "6696B1DA27BEA173B5EFCAABBB8492A9"
	                     "2AE3D97F7EE3C7314FB7E2FF8AEFD329"
	                     "F5F8202C22650BB79A7D9F3867F00000"
	                     "00000000000000000000000000000");
	check_multiply_pow10("123456789ABCDEF0",
	                     176,
	                     "1CC05FF0499D8BC7D8EBE0C6DC2FDC09"
	                     "E93765F3448235FB16AD09D98BBB3A0A"
	                     "843372D33A318EE63DAE6998DA59EF34"
	                     "B15C40A65B9B65ABF3CAF00000000000"
	                     "00000000000000000000000000000000"
	                     "00");
	check_multiply_pow10("123456789ABCDEF0",
	                     220,
	                     "80ED0FD9A6C0F56A495F466320D34E22"
	                     "507FAA83F0519E7FF909FDDBDA184682"
	                     "BB70D38D43284C828A3681540722E550"
	                     "960567BAB1C25389C1BE7705228BE8CC"
	                     "AF3EBD382829DF000000000000000000"
	                     "00000000000000000000000000000000"
	                     "000000");
	check_multiply_pow10("123456789ABCDEF0",
	                     264,
	                     "2421FD0F55C486D05211339D45EC2DC4"
	                     "12AE7A64DDFE619DA81B73C069088D3E"
	                     "83D7AA9F99B571815DE939A5275FB4A6"
	                     "9D8930798C01FB96781B9D633BB59AD5"
	                     "A7F322A7EC14154D1B8B5DF1718779A5"
	                     "2291FE0F000000000000000000000000"
	                     "00000000000000000000000000000000"
	                     "00000000000");
	check_multiply_pow10("123456789ABCDEF0",
	                     308,
	                     "A206620F35C83E9E780ECC07DCAF13BB"
	                     "0A7EE2E213747914340BC172D783BA56"
	                     "661E8DCFFD03C398BD66F5570F445AC6"
	                     "737126283C64AE1A289B9D8BB4531033"
	                     "8C3E34DE2D534187092ABA1F4706100E"
	                     "ECF66D14059461A05A9BEBBCCBA0F693"
	                     "F0000000000000000000000000000000"
	                     "00000000000000000000000000000000"
	                     "000000000000000");
}

static void
check_divmod(const char*    lhs_hex,
             const char*    rhs_hex,
             const uint32_t expected_divisor,
             const char*    expected_mod_hex)
{
	SerdBigint       lhs     = bigint_from_hex(lhs_hex);
	const SerdBigint rhs     = bigint_from_hex(rhs_hex);
	const uint32_t   divisor = serd_bigint_divmod(&lhs, &rhs);

	assert(divisor == expected_divisor);
	CHECK_HEXEQ(expected_mod_hex, &lhs);
}

static void
test_divmod(void)
{
	check_divmod("A", "2", 5, "0");
	check_divmod("B", "2", 5, "1");
	check_divmod("C", "2", 6, "0");
	check_divmod("A", "1234567890", 0, "A");
	check_divmod("FFFFFFFF", "3", 0x55555555, "0");
	check_divmod("12345678", "3789012", 5, "D9861E");
	check_divmod("70000001", "1FFFFFFF", 3, "10000004");
	check_divmod("28000000", "12A05F20", 2, "2BF41C0");
	check_divmod("FFFFFFFFF", "FFFFFFFF", 16, "F");
	check_divmod("100000000000001", "FFFFFFF", 0x10000001, "2");
	check_divmod("40000000000002", "2FAF0800000000", 1, "1050F800000002");
	check_divmod("40000000000000", "40000000000000", 1, "0");

	check_divmod("43DE72C3DF858FC278A361EEB5A000000",
	             "80000000000000000000000000000000",
	             8,
	             "3DE72C3DF858FC278A361EEB5A000000");

	check_divmod("B5C5AF0376E3C800000",
	             "43C33C1937564800000",
	             2,
	             "2E3F36D108373800000");


	check_divmod("A0000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "000000000000000000000000000000",
	             "20000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "000000000000000000000000000000",
	             5,
	             "0");

	check_divmod("A0000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "000000000000000000000000000001",
	             "20000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "000000000000000000000000000000",
	             5,
	             "1");

	check_divmod("B6080000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "000000000000000000000000000000FF"
	             "F",
	             "A0000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "000000000000000000000000000000",
	             0x1234,
	             "FFF");

	check_divmod("B6080000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "00000000000000000000000000000000"
	             "0",
	             "9FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
	             "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
	             "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
	             "FFFFFFFFFFFFFFFFFFFFFFFFFFF001",
	             0x1234,
	             "1232DCC");
}

static void
check_compare(const char* lhs_hex, const char* rhs_hex, const int expected_cmp)
{
	const SerdBigint lhs = bigint_from_hex(lhs_hex);
	const SerdBigint rhs = bigint_from_hex(rhs_hex);
	const int        cmp = serd_bigint_compare(&lhs, &rhs);

	assert(cmp == expected_cmp);

	if (cmp) {
		const int rcmp = serd_bigint_compare(&rhs, &lhs);
		assert(rcmp == -cmp);
	}
}

static void
test_compare(void)
{
	check_compare("1", "1", 0);
	check_compare("0", "1", -1);
	check_compare("F", "FF", -1);
	check_compare("F", "FFFFFFFFF", -1);
	check_compare("10000000000000000", "10000000000000000", 0);
	check_compare("10000000000000000", "10000000000000001", -1);
	check_compare("FFFFFFFFFFFFFFFFF", "100000000000000000", -1);
	check_compare("10000000000000000", "10000000000000001", -1);
	check_compare("1234567890ABCDEF12345", "1234567890ABCDEF12345", 0);
	check_compare("1234567890ABCDEF12345", "1234567890ABCDEF12346", -1);

	const char* const huge = "123456789ABCDEF0123456789ABCDEF0"
	                         "123456789ABCDEF0123456789ABCDEF0"
	                         "123456789ABCDEF0123456789ABCDEF0"
	                         "123456789ABCDEF0123456789ABCDEF0";

	const char* const huger = "123456789ABCDEF0123456789ABCDEF0"
	                          "123456789ABCDEF0123456789ABCDEF0"
	                          "123456789ABCDEF0123456789ABCDEF0"
	                          "123456789ABCDEF0123456789ABCDEF1";

	check_compare(huge, huge, 0);
	check_compare(huger, huger, 0);
	check_compare(huge, huger, -1);
}

static void
check_plus_compare(const char* l_hex,
                   const char* p_hex,
                   const char* c_hex,
                   const int   expected_cmp)
{
	const SerdBigint l    = bigint_from_hex(l_hex);
	const SerdBigint p    = bigint_from_hex(p_hex);
	const SerdBigint c    = bigint_from_hex(c_hex);
	const int        cmp  = serd_bigint_plus_compare(&l, &p, &c);
	const int        rcmp = serd_bigint_plus_compare(&p, &l, &c);

	assert(cmp == expected_cmp);
	assert(rcmp == expected_cmp);
}

static void
test_plus_compare(void)
{
	check_plus_compare("1", "0", "1", 0);
	check_plus_compare("0", "0", "1", -1);
	check_plus_compare("FFFFFFFFF", "F", "F", 1);
	check_plus_compare("F", "F", "800000000", -1);
	check_plus_compare("F", "F", "80000000000000000", -1);
	check_plus_compare("800000000", "F", "80000000000000000", -1);
	check_plus_compare("2D79883D20000", "2D79883D20000", "5AF3107A40000", 0);
	check_plus_compare("20000000000000", "1", "20000000000000", +1);

	check_plus_compare("0588A503282FE00000",
	                   "0588A503282FE00000",
	                   "0AD78EBC5AC6200000",
	                   +1);

	check_plus_compare("2F06018572BEADD1280000000",
	                   "0204FCE5E3E25026110000000",
	                   "4000000000000000000000000",
	                   -1);

	check_plus_compare("1234567890ABCDEF12345",
	                   "000000000000000000001",
	                   "1234567890ABCDEF12345",
	                   +1);

	check_plus_compare("1234567890ABCDEF12344",
	                   "000000000000000000001",
	                   "1234567890ABCDEF12345",
	                   0);

	check_plus_compare("123456789000000000000",
	                   "0000000000ABCDEF12345",
	                   "1234567890ABCDEF12345",
	                   0);

	check_plus_compare("123456789000000000000",
	                   "0000000000ABCDEF12344",
	                   "1234567890ABCDEF12345",
	                   -1);

	check_plus_compare("123456789000000000000",
	                   "0000000000ABCDEF12346",
	                   "1234567890ABCDEF12345",
	                   1);

	check_plus_compare("123456789100000000000",
	                   "0000000000ABCDEF12345",
	                   "1234567890ABCDEF12345",
	                   1);

	check_plus_compare("123456788900000000000",
	                   "0000000000ABCDEF12345",
	                   "1234567890ABCDEF12345",
	                   -1);

	check_plus_compare("12345678900000000000000000000",
	                   "0000000000ABCDEF1234500000000",
	                   "1234567890ABCDEF1234500000000",
	                   0);

	check_plus_compare("12345678900000000000000000000",
	                   "0000000000ABCDEF1234400000000",
	                   "1234567890ABCDEF1234500000000",
	                   -1);

	check_plus_compare("12345678900000000000000000000",
	                   "0000000000ABCDEF1234600000000",
	                   "1234567890ABCDEF1234500000000",
	                   1);

	check_plus_compare("12345678910000000000000000000",
	                   "0000000000ABCDEF1234500000000",
	                   "1234567890ABCDEF1234500000000",
	                   1);
	check_plus_compare("12345678890000000000000000000",
	                   "0000000000ABCDEF1234500000000",
	                   "1234567890ABCDEF1234500000000",
	                   -1);

	check_plus_compare("12345678900000000000000000000",
	                   "000000000000000000ABCDEF12345",
	                   "123456789000000000ABCDEF12345",
	                   0);

	check_plus_compare("12345678900000000000000000000",
	                   "000000000000000000ABCDEF12346",
	                   "123456789000000000ABCDEF12345",
	                   1);

	check_plus_compare("12345678900000000000000000000",
	                   "000000000000000000ABCDEF12344",
	                   "123456789000000000ABCDEF12345",
	                   -1);

	check_plus_compare("12345678900000000000000000000",
	                   "000000000000000000ABCDEF12345",
	                   "12345678900000ABCDEF123450000",
	                   -1);

	check_plus_compare("12345678900000000000000000000",
	                   "000000000000000000ABCDEF12344",
	                   "12345678900000ABCDEF123450000",
	                   -1);

	check_plus_compare("12345678900000000000000000000",
	                   "000000000000000000ABCDEF12345",
	                   "12345678900000ABCDEF123450001",
	                   -1);

	check_plus_compare("12345678900000000000000000000",
	                   "00000000000000ABCDEF123460000",
	                   "12345678900000ABCDEF123450000",
	                   1);
}

static void
check_pow10(const unsigned exponent, const char* expected)
{
	SerdBigint num;
	serd_bigint_set_pow10(&num, exponent);
	CHECK_HEXEQ(expected, &num);
}

static void
test_set_pow10(void)
{
	check_pow10(0, "1");
	check_pow10(1, "A");
	check_pow10(2, "64");
	check_pow10(5, "186A0");
	check_pow10(8, "5F5E100");
	check_pow10(16, "2386F26FC10000");
	check_pow10(30, "C9F2C9CD04674EDEA40000000");
	check_pow10(31, "7E37BE2022C0914B2680000000");
}

int
main(void)
{
	test_set();
	test_output();
	test_left_shifted_bigit();
	test_shift_left();
	test_add_u32();
	test_add();
	test_subtract();
	test_subtract_left_shifted();
	test_multiply_u32();
	test_multiply_u64();
	test_multiply_pow10();
	test_divmod();
	test_compare();
	test_plus_compare();
	test_set_pow10();

	return 0;
}
