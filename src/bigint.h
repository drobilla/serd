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

#ifndef SERD_BIGINT_H
#define SERD_BIGINT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t Bigit;

/* We need enough precision for any double, the "largest" of which (using
   absolute exponents) is the smallest subnormal ~= 5e-324.  This is 1076 bits
   long, but we need a bit more space for arithmetic.  This is absurd, but such
   is decimal.  These are only used on the stack so it doesn't hurt too much.
*/

#define BIGINT_MAX_SIGNIFICANT_BITS 1280
#define BIGINT_BIGIT_BITS 32
#define BIGINT_MAX_BIGITS (BIGINT_MAX_SIGNIFICANT_BITS / BIGINT_BIGIT_BITS)

typedef struct
{
	Bigit    bigits[BIGINT_MAX_BIGITS];
	unsigned n_bigits;
} SerdBigint;

void
serd_bigint_zero(SerdBigint* num);

size_t
serd_bigint_print_hex(FILE* stream, const SerdBigint* num);

size_t
serd_bigint_to_hex_string(const SerdBigint* num, char* buf, size_t len);

void
serd_bigint_clamp(SerdBigint* num);

void
serd_bigint_shift_left(SerdBigint* num, unsigned amount);

void
serd_bigint_set(SerdBigint* num, const SerdBigint* value);

void
serd_bigint_set_u32(SerdBigint* num, uint32_t value);

void
serd_bigint_set_u64(SerdBigint* num, uint64_t value);

void
serd_bigint_set_pow10(SerdBigint* num, unsigned exponent);

void
serd_bigint_set_decimal_string(SerdBigint* num, const char* str);

void
serd_bigint_set_hex_string(SerdBigint* num, const char* str);

void
serd_bigint_multiply_u32(SerdBigint* num, uint32_t factor);

void
serd_bigint_multiply_u64(SerdBigint* num, uint64_t factor);

void
serd_bigint_multiply_pow10(SerdBigint* num, unsigned exponent);

int
serd_bigint_compare(const SerdBigint* lhs, const SerdBigint* rhs);

void
serd_bigint_add_u32(SerdBigint* lhs, uint32_t rhs);

void
serd_bigint_add(SerdBigint* lhs, const SerdBigint* rhs);

void
serd_bigint_subtract(SerdBigint* lhs, const SerdBigint* rhs);

Bigit
serd_bigint_left_shifted_bigit(const SerdBigint* num,
                               unsigned          amount,
                               unsigned          index);

/// Faster implementation of serd_bigint_subtract(lhs, rhs << amount)
void
serd_bigint_subtract_left_shifted(SerdBigint*       lhs,
                                  const SerdBigint* rhs,
                                  unsigned          amount);

/// Faster implementation of serd_bigint_compare(l + p, c)
int
serd_bigint_plus_compare(const SerdBigint* l,
                         const SerdBigint* p,
                         const SerdBigint* c);

/// Divide and set `lhs` to modulo
uint32_t
serd_bigint_divmod(SerdBigint* lhs, const SerdBigint* rhs);

#endif // SERD_BIGINT_H
