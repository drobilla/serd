/*
  Copyright 2019 David Robillard <http://drobilla.net>

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

#include "bigint.h"

#include "int_math.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t Hugit;

static const uint32_t bigit_mask = ~(uint32_t)0;
static const uint64_t carry_mask = (uint64_t)~(uint32_t)0 << 32;

typedef struct
{
	unsigned bigits;
	unsigned bits;
} Offset;

static inline Offset
make_offset(const unsigned i)
{
	const unsigned bigits = i / BIGINT_BIGIT_BITS;
	const unsigned bits   = i - bigits * BIGINT_BIGIT_BITS;

	const Offset offset = {bigits, bits};
	return offset;
}

#ifndef NDEBUG
static inline bool
serd_bigint_is_clamped(const SerdBigint* num)
{
	return num->n_bigits == 0 || num->bigits[num->n_bigits - 1];
}
#endif

size_t
serd_bigint_print_hex(FILE* const stream, const SerdBigint* const num)
{
	assert(serd_bigint_is_clamped(num));
	if (num->n_bigits == 0) {
		fprintf(stream, "0");
		return 1;
	}

	int len = fprintf(stream, "%X", num->bigits[num->n_bigits - 1]);
	for (unsigned i = 1; i < num->n_bigits; ++i) {
		len += fprintf(stream, "%08X", num->bigits[num->n_bigits - 1 - i]);
	}

	return (size_t)len;
}

size_t
serd_bigint_to_hex_string(const SerdBigint* const num,
                          char* const             buf,
                          const size_t            len)
{
	if (len < 9) {
		return 0;
	} else if (num->n_bigits == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return 1;
	}

	size_t n = (size_t)snprintf(buf, len, "%X", num->bigits[num->n_bigits - 1]);
	for (unsigned i = 1; n + 8 <= len && i < num->n_bigits; ++i) {
		n += (size_t)snprintf(buf + n,
		                      len - n,
		                      "%08X",
		                      num->bigits[num->n_bigits - 1 - i]);
	}

	return n;
}

void
serd_bigint_shift_left(SerdBigint* num, const unsigned amount)
{
	assert(serd_bigint_is_clamped(num));
	if (amount == 0 || num->n_bigits == 0) {
		return;
	}

	const Offset offset = make_offset(amount);

	assert(num->n_bigits + offset.bigits < BIGINT_MAX_BIGITS);
	num->n_bigits += offset.bigits + (bool)offset.bits;

	if (offset.bits == 0) { // Simple bigit-aligned shift
		for (unsigned i = num->n_bigits - 1; i >= offset.bigits; --i) {
			num->bigits[i] = num->bigits[i - offset.bigits];
		}
	} else { // Bigit + sub-bigit bit offset shift
		const unsigned right_shift = BIGINT_BIGIT_BITS - offset.bits;
		for (unsigned i = num->n_bigits - offset.bigits - 1; i > 0; --i) {
			num->bigits[i + offset.bigits] =
			        (num->bigits[i] << offset.bits) |
			        (num->bigits[i - 1] >> right_shift);
		}

		num->bigits[offset.bigits] = num->bigits[0] << offset.bits;
	}

	// Zero LSBs
	for (unsigned i = 0; i < offset.bigits; ++i) {
		num->bigits[i] = 0;
	}

	serd_bigint_clamp(num);
	assert(serd_bigint_is_clamped(num));
}

void
serd_bigint_zero(SerdBigint* num)
{
	static const SerdBigint zero = {{0}, 0};

	*num = zero;
}

void
serd_bigint_set(SerdBigint* num, const SerdBigint* value)
{
	*num = *value;
}

void
serd_bigint_set_u32(SerdBigint* num, const uint32_t value)
{
	serd_bigint_zero(num);

	num->bigits[0] = value;
	num->n_bigits  = (bool)value;
}

void
serd_bigint_clamp(SerdBigint* num)
{
	while (num->n_bigits > 0 && num->bigits[num->n_bigits - 1] == 0) {
		--num->n_bigits;
	}
}

void
serd_bigint_set_u64(SerdBigint* num, const uint64_t value)
{
	serd_bigint_zero(num);

	num->bigits[0] = value & bigit_mask;
	num->bigits[1] = value >> BIGINT_BIGIT_BITS;
	num->n_bigits  = num->bigits[1] ? 2 : num->bigits[0] ? 1 : 0;
}

void
serd_bigint_set_pow10(SerdBigint* num, const unsigned exponent)
{
	serd_bigint_set_u32(num, 1);
	serd_bigint_multiply_pow10(num, exponent);
}

static uint32_t
read_u32(const char* const str, uint32_t* result, uint32_t* n_digits)
{
	static const size_t uint32_digits10 = 9;

	*result = *n_digits = 0;

	uint32_t i = 0;
	for (; str[i] && *n_digits < uint32_digits10; ++i) {
		if (str[i] >= '0' && str[i] <= '9') {
			*result = *result * 10u + (unsigned)(str[i] - '0');
			*n_digits += 1;
		} else if (str[i] != '.') {
			break;
		}
	}

	return i;
}

void
serd_bigint_set_decimal_string(SerdBigint* num, const char* const str)
{
	serd_bigint_zero(num);

	uint32_t pos      = 0;
	uint32_t n_digits = 0;
	uint32_t n_read   = 0;
	uint32_t word     = 0;
	while ((n_read = read_u32(str + pos, &word, &n_digits))) {
		serd_bigint_multiply_u32(num, (uint32_t)POW10[n_digits]);
		serd_bigint_add_u32(num, word);
		pos += n_read;
	}

	serd_bigint_clamp(num);
}

void
serd_bigint_set_hex_string(SerdBigint* num, const char* const str)
{
	serd_bigint_zero(num);

	// Read digits from right to left until we run off the beginning
	const int length       = (int)strlen(str);
	char      digit_buf[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
	int       i            = length - 8;
	for (; i >= 0; i -= 8) {
		memcpy(digit_buf, str + i, 8);
		num->bigits[num->n_bigits++] = (Bigit)strtoll(digit_buf, NULL, 16);
	}

	// Read leftovers into MSB if necessary
	if (i > -8) {
		memset(digit_buf, 0, sizeof(digit_buf));
		memcpy(digit_buf, str, 8u + (unsigned)i);
		num->bigits[num->n_bigits++] = (Bigit)strtoll(digit_buf, NULL, 16);
	}

	serd_bigint_clamp(num);
}

void
serd_bigint_multiply_u32(SerdBigint* num, const uint32_t factor)
{
	switch (factor) {
	case 0: serd_bigint_zero(num); return;
	case 1: return;
	}

	Hugit carry = 0;
	for (unsigned i = 0; i < num->n_bigits; ++i) {
		const Hugit p     = (Hugit)factor * num->bigits[i];
		const Hugit hugit = p + (carry & bigit_mask);

		num->bigits[i] = hugit & bigit_mask;

		carry = (hugit >> 32) + (carry >> 32);
	}

	for (; carry; carry >>= 32) {
		assert(num->n_bigits + 1 <= BIGINT_MAX_BIGITS);
		num->bigits[num->n_bigits++] = (Bigit)carry;
	}
}

void
serd_bigint_multiply_u64(SerdBigint* num, const uint64_t factor)
{
	switch (factor) {
	case 0: serd_bigint_zero(num); return;
	case 1: return;
	}

	const Hugit f_lo = factor & bigit_mask;
	const Hugit f_hi = factor >> 32;

	Hugit carry = 0;
	for (unsigned i = 0; i < num->n_bigits; ++i) {
		const Hugit p_lo  = f_lo * num->bigits[i];
		const Hugit p_hi  = f_hi * num->bigits[i];
		const Hugit hugit = p_lo + (carry & bigit_mask);

		num->bigits[i] = hugit & bigit_mask;
		carry          = p_hi + (hugit >> 32) + (carry >> 32);
	}

	for (; carry; carry >>= 32) {
		assert(num->n_bigits + 1 <= BIGINT_MAX_BIGITS);
		num->bigits[num->n_bigits++] = carry & bigit_mask;
	}
}

void
serd_bigint_multiply_pow10(SerdBigint* num, const unsigned exponent)
{
	/* To reduce multiplication, we exploit 10^e = (2*5)^e = 2^e * 5^e to
	   factor out an exponentiation by 5 instead of 10.  So, we first multiply
	   by 5^e (hard), then by 2^e (just a single left shift). */

	// 5^27, the largest power of 5 that fits in 64 bits
	static const uint64_t pow5_27 = 7450580596923828125ull;

	// Powers of 5 up to 5^13, the largest that fits in 32 bits
	static const uint32_t pow5[] = {
	        1,
	        5,
	        5 * 5,
	        5 * 5 * 5,
	        5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
	        5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
	};

	if (exponent == 0 || num->n_bigits == 0) {
		return;
	}

	// Multiply by 5^27 until e < 27 so we can switch to 32 bits
	unsigned e = exponent;
	while (e >= 27) {
		serd_bigint_multiply_u64(num, pow5_27);
		e -= 27;
	}

	// Multiply by 5^13 until e < 13 so we have only one multiplication left
	while (e >= 13) {
		serd_bigint_multiply_u32(num, pow5[13]);
		e -= 13;
	}

	// Multiply by the final 5^e (which may be zero, making this a noop)
	serd_bigint_multiply_u32(num, pow5[e]);

	// Finally multiply by 2^e
	serd_bigint_shift_left(num, exponent);
}

int
serd_bigint_compare(const SerdBigint* lhs, const SerdBigint* rhs)
{
	if (lhs->n_bigits < rhs->n_bigits) {
		return -1;
	} else if (lhs->n_bigits > rhs->n_bigits) {
		return 1;
	}

	for (int i = (int)lhs->n_bigits - 1; i >= 0; --i) {
		const Bigit bigit_l = lhs->bigits[i];
		const Bigit bigit_r = rhs->bigits[i];
		if (bigit_l < bigit_r) {
			return -1;
		} else if (bigit_l > bigit_r) {
			return 1;
		}
	}

	return 0;
}

int
serd_bigint_plus_compare(const SerdBigint* l,
                         const SerdBigint* p,
                         const SerdBigint* c)
{
	assert(serd_bigint_is_clamped(l));
	assert(serd_bigint_is_clamped(p));
	assert(serd_bigint_is_clamped(c));

	if (l->n_bigits < p->n_bigits) {
		return serd_bigint_plus_compare(p, l, c);
	} else if (l->n_bigits + 1 < c->n_bigits) {
		return -1;
	} else if (l->n_bigits > c->n_bigits) {
		return 1;
	} else if (p->n_bigits < l->n_bigits && l->n_bigits < c->n_bigits) {
		return -1;
	}

	Hugit borrow = 0;
	for (int i = (int)c->n_bigits - 1; i >= 0; --i) {
		const Bigit ai  = l->bigits[i];
		const Bigit bi  = p->bigits[i];
		const Bigit ci  = c->bigits[i];
		const Hugit sum = (Hugit)ai + bi;

		if (sum > ci + borrow) {
			return 1;
		} else if ((borrow += ci - sum) > 1) {
			return -1;
		}

		borrow <<= 32;
	}

	return borrow ? -1 : 0;
}

void
serd_bigint_add_u32(SerdBigint* lhs, const uint32_t rhs)
{
	if (lhs->n_bigits == 0) {
		serd_bigint_set_u32(lhs, rhs);
		return;
	}

	Hugit sum   = (Hugit)lhs->bigits[0] + rhs;
	Bigit carry = sum >> 32;

	lhs->bigits[0] = sum & bigit_mask;

	unsigned i = 1;
	for (; carry; ++i) {
		assert(carry == 0 || carry == 1);

		sum            = (Hugit)carry + lhs->bigits[i];
		lhs->bigits[i] = sum & bigit_mask;
		carry          = (sum & carry_mask) >> 32;
	}

	lhs->n_bigits = MAX(i, lhs->n_bigits);
	assert(serd_bigint_is_clamped(lhs));
}

void
serd_bigint_add(SerdBigint* lhs, const SerdBigint* rhs)
{
	assert(MAX(lhs->n_bigits, rhs->n_bigits) + 1 <= BIGINT_MAX_BIGITS);

	bool     carry = 0;
	unsigned i     = 0;
	for (; i < rhs->n_bigits; ++i) {
		const Hugit sum = (Hugit)lhs->bigits[i] + rhs->bigits[i] + carry;

		lhs->bigits[i] = sum & bigit_mask;
		carry          = (sum & carry_mask) >> 32;
	}

	for (; carry; ++i) {
		const Hugit sum = (Hugit)lhs->bigits[i] + carry;

		lhs->bigits[i] = sum & bigit_mask;
		carry          = (sum & carry_mask) >> 32;
	}

	lhs->n_bigits = MAX(i, lhs->n_bigits);
	assert(serd_bigint_is_clamped(lhs));
}

void
serd_bigint_subtract(SerdBigint* lhs, const SerdBigint* rhs)
{
	assert(serd_bigint_is_clamped(lhs));
	assert(serd_bigint_is_clamped(rhs));
	assert(serd_bigint_compare(lhs, rhs) >= 0);

	bool     borrow = 0;
	unsigned i;
	for (i = 0; i < rhs->n_bigits; ++i) {
		const Bigit l = lhs->bigits[i];
		const Bigit r = rhs->bigits[i];

		lhs->bigits[i] = l - r - borrow;
		borrow         = l < r || (l == r && borrow);
	}

	for (; borrow; ++i) {
		const Bigit l = lhs->bigits[i];

		lhs->bigits[i] -= borrow;

		borrow = l == 0;
	}

	serd_bigint_clamp(lhs);
}

static unsigned
serd_bigint_leading_zeros(const SerdBigint* num)
{
	return 32 * (BIGINT_MAX_BIGITS - num->n_bigits) +
	       serd_clz32(num->bigits[num->n_bigits - 1]);
}

static Bigit
serd_bigint_left_shifted_bigit_i(const SerdBigint* num,
                                 const Offset      amount,
                                 const unsigned    index)
{
	assert(serd_bigint_is_clamped(num));
	if (amount.bigits == 0 && amount.bits == 0) {
		return num->bigits[index];
	}

	if (index < amount.bigits) {
		return 0;
	}

	if (amount.bits == 0) { // Simple bigit-aligned shift
		return num->bigits[index - amount.bigits];
	} else if (index == amount.bigits) { // Last non-zero bigit
		return num->bigits[0] << amount.bits;
	} else { // Bigit + sub-bigit bit offset shift
		const unsigned right_shift = BIGINT_BIGIT_BITS - amount.bits;
		return (num->bigits[index - amount.bigits] << amount.bits) |
		       (num->bigits[index - amount.bigits - 1] >> right_shift);
	}
}

Bigit
serd_bigint_left_shifted_bigit(const SerdBigint* num,
                               const unsigned    amount,
                               const unsigned    index)
{
	return serd_bigint_left_shifted_bigit_i(num, make_offset(amount), index);
}

void
serd_bigint_subtract_left_shifted(SerdBigint*       lhs,
                                  const SerdBigint* rhs,
                                  const unsigned    amount)
{
	assert(serd_bigint_is_clamped(lhs));
	assert(serd_bigint_is_clamped(rhs));
#ifndef NDEBUG
	{
		SerdBigint check_rhs = *rhs;
		serd_bigint_shift_left(&check_rhs, amount);
		assert(serd_bigint_compare(lhs, &check_rhs) >= 0);
	}
#endif

	const Offset   offset = make_offset(amount);
	const unsigned r_n_bigits =
	        rhs->n_bigits + offset.bigits + (bool)offset.bits;

	bool     borrow = 0;
	unsigned i;
	for (i = 0; i < r_n_bigits; ++i) {
		const Bigit l = lhs->bigits[i];
		const Bigit r = serd_bigint_left_shifted_bigit_i(rhs, offset, i);

		lhs->bigits[i] = l - r - borrow;
		borrow         = l < r || ((l == r) && borrow);
	}

	for (; borrow; ++i) {
		const Bigit l = lhs->bigits[i];

		lhs->bigits[i] -= borrow;

		borrow = l == 0;
	}

	serd_bigint_clamp(lhs);
}

uint32_t
serd_bigint_divmod(SerdBigint* lhs, const SerdBigint* rhs)
{
	assert(serd_bigint_is_clamped(lhs));
	assert(serd_bigint_is_clamped(rhs));
	assert(rhs->n_bigits > 0);
	if (lhs->n_bigits < rhs->n_bigits) {
		return 0;
	}

	uint32_t       result = 0;
	const Bigit    r0     = rhs->bigits[rhs->n_bigits - 1];
	const unsigned rlz    = serd_bigint_leading_zeros(rhs);

	// Shift and subtract until the LHS does not have more bigits
	int big_steps = 0;
	while (lhs->n_bigits > rhs->n_bigits) {
		const unsigned llz   = serd_bigint_leading_zeros(lhs);
		const unsigned shift = rlz - llz - 1;

		result += 1u << shift;
		serd_bigint_subtract_left_shifted(lhs, rhs, shift);
		++big_steps;
	}

	// Handle simple termination cases
	int cmp = serd_bigint_compare(lhs, rhs);
	if (cmp < 0) {
		return result;
	} else if (cmp > 0 && lhs->n_bigits == 1) {
		assert(rhs->n_bigits == 1);
		const Bigit l0 = lhs->bigits[lhs->n_bigits - 1];

		lhs->bigits[lhs->n_bigits - 1] = l0 % r0;
		lhs->n_bigits -= (lhs->bigits[lhs->n_bigits - 1] == 0);
		return result + l0 / r0;
	}

	// Both now have the same number of digits, finish with subtraction
	int final_steps = 0;
	for (; cmp >= 0; cmp = serd_bigint_compare(lhs, rhs)) {
		const unsigned llz = serd_bigint_leading_zeros(lhs);
		if (rlz == llz) {
			// Both have the same number of leading zeros, just subtract
			serd_bigint_subtract(lhs, rhs);
			return result + 1;
		}

		const unsigned shift = rlz - llz - 1;
		result += 1u << shift;
		serd_bigint_subtract_left_shifted(lhs, rhs, shift);
		++final_steps;
	}

	return result;
}
