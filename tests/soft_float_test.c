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

#include "test_data.h"

#include "../src/ieee_float.h"
#include "../src/soft_float.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static uint64_t
ulp_distance(const double a, const double b)
{
	assert(a >= 0.0);
	assert(b >= 0.0);

	if (a == b) {
		return 0;
	} else if (isnan(a) || isnan(b)) {
		return UINT64_MAX;
	} else if (isinf(a) || isinf(b)) {
		return UINT64_MAX;
	}

	const uint64_t ia = double_to_rep(a);
	const uint64_t ib = double_to_rep(b);

	return ia > ib ? ia - ib : ib - ia;
}

static bool
check_multiply(const double lhs, const double rhs)
{
	assert(lhs >= 0.0);
	assert(rhs >= 0.0);

	const SerdSoftFloat sl = soft_float_normalize(soft_float_from_double(lhs));
	const SerdSoftFloat sr = soft_float_normalize(soft_float_from_double(rhs));
	const SerdSoftFloat sp = soft_float_multiply(sl, sr);
	const double        ep = lhs * rhs;
	const double        dp = soft_float_to_double(sp);

	return ulp_distance(dp, ep) <= 1;
}

static void
test_multiply(void)
{
	assert(check_multiply(1.0, 1.0));
	assert(check_multiply(1.0, 8.0));
	assert(check_multiply(8.0, 1.0));
	assert(check_multiply(2.0, 4.0));
	assert(check_multiply(1e100, 1e-100));

	uint64_t seed = 1;
	for (int i = 0; i < 1000000; ++i) {
		const double l = fabs(double_from_rep(seed = lcg64(seed)));
		const double r = fabs(double_from_rep(seed = lcg64(seed)));
		if (isfinite(l) && isfinite(r)) {
			assert(check_multiply(l, r));
		}
	}
}

static void
test_exact_pow10(void)
{
	for (int i = 1; i < dec_expt_step; ++i) {
		const SerdSoftFloat power = soft_float_exact_pow10(i);

		const double d = soft_float_to_double(power);
		const double p = pow(10, i);
		assert(ulp_distance(d, p) <= 1);
	}
}

static void
test_pow10_under(void)
{
	for (int i = min_dec_expt; i < max_dec_expt + dec_expt_step; ++i) {
		int                 expt10 = 0;
		const SerdSoftFloat power  = soft_float_pow10_under(i, &expt10);

		assert(expt10 <= i);
		assert(i - expt10 < dec_expt_step);

		const double d = soft_float_to_double(power);
		const double p = pow(10, expt10);
		assert(ulp_distance(d, p) <= 1);
	}
}

int
main(void)
{
	test_multiply();
	test_exact_pow10();
	test_pow10_under();

	return 0;
}
