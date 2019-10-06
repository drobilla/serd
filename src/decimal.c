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

#include "decimal.h"

#include "int_math.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stddef.h>

int
serd_count_digits(const uint64_t i)
{
	return i == 0 ? 1 : (int)serd_ilog10(i) + 1;
}

unsigned
serd_double_int_digits(const double abs)
{
	const double lg = ceil(log10(floor(abs) + 1.0));
	return lg < 1.0 ? 1U : (unsigned)lg;
}

unsigned
serd_decimals(const double d, char* const buf, const unsigned frac_digits)
{
	assert(isfinite(d));

	// Point s to decimal point location
	const double   abs_d      = fabs(d);
	const double   int_part   = floor(abs_d);
	const unsigned int_digits = serd_double_int_digits(abs_d);
	unsigned       n_bytes    = 0;
	char*          s          = buf + int_digits;
	if (d < 0.0) {
		*buf = '-';
		++s;
	}

	// Write integer part (right to left)
	char*    t   = s - 1;
	uint64_t dec = (uint64_t)int_part;
	do {
		*t-- = (char)('0' + dec % 10);
	} while ((dec /= 10) > 0);


	*s++ = '.';

	// Write fractional part (right to left)
	double frac_part = fabs(d - int_part);
	if (frac_part < DBL_EPSILON) {
		*s++ = '0';
		n_bytes = (size_t)(s - buf);
	} else {
		uint64_t frac = (uint64_t)llround(frac_part * pow(10.0, (int)frac_digits));
		s += frac_digits - 1;
		unsigned i = 0;

		// Skip trailing zeros
		for (; i < frac_digits - 1 && !(frac % 10); ++i, --s, frac /= 10) {}

		n_bytes = (size_t)(s - buf) + 1u;

		// Write digits from last trailing zero to decimal point
		for (; i < frac_digits; ++i) {
			*s-- = (char)('0' + (frac % 10));
			frac /= 10;
		}
	}

	return n_bytes;
}
