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

#include "string.h"

#include "bigint.h"
#include "ieee_float.h"
#include "int_math.h"
#include "serd/serd.h"
#include "soft_float.h"
#include "string_utils.h"

#include "serd/serd.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static const int uint64_digits10 = 19;

void
serd_free(void* ptr)
{
	free(ptr);
}

const char*
serd_strerror(SerdStatus status)
{
	switch (status) {
	case SERD_SUCCESS:        return "Success";
	case SERD_FAILURE:        return "Non-fatal failure";
	case SERD_ERR_UNKNOWN:    return "Unknown error";
	case SERD_ERR_BAD_SYNTAX: return "Invalid syntax";
	case SERD_ERR_BAD_ARG:    return "Invalid argument";
	case SERD_ERR_BAD_ITER:   return "Invalid iterator";
	case SERD_ERR_NOT_FOUND:  return "Not found";
	case SERD_ERR_ID_CLASH:   return "Blank node ID clash";
	case SERD_ERR_BAD_CURIE:  return "Invalid CURIE";
	case SERD_ERR_INTERNAL:   return "Internal error";
	case SERD_ERR_OVERFLOW:   return "Stack overflow";
	case SERD_ERR_INVALID:    return "Invalid data";
	case SERD_ERR_NO_DATA:    return "Unexpectd end of input";
	case SERD_ERR_BAD_WRITE:  return "Error writing to file";
	}
	return "Unknown error";  // never reached
}

size_t
serd_strlen(const char* str, SerdNodeFlags* flags)
{
	if (flags) {
		size_t i = 0;
		*flags = 0;
		for (; str[i]; ++i) {
			serd_update_flags(str[i], flags);
		}
		return i;
	}

	return strlen(str);
}

static inline int
read_sign(const char** sptr)
{
	int sign = 1;
	switch (**sptr) {
	case '-':
		sign = -1;
		// fallthru
	case '+':
		++(*sptr);
		// fallthru
	default:
		return sign;
	}
}

typedef struct
{
	int         sign;        ///< Sign (+1 or -1)
	int         digits_expt; ///< Exponent for digits
	const char* digits;      ///< Pointer to the first digit in the significand
	uint64_t    frac;        ///< Significand
	int         frac_expt;   ///< Exponent for frac
	int         n_digits;    ///< Number of digits in the significand
	size_t      end;         ///< Index of the last read character
} SerdParsedDouble;

static SerdParsedDouble
serd_parse_double(const char* const str)
{
	// Read leading sign if necessary
	const char* s    = str;
	const int   sign = read_sign(&s);

	// Skip leading zeros before decimal point
	while (*s == '0') {
		++s;
	}

	// Skip leading zeros after decimal point
	int  n_leading   = 0;     // Zeros skipped after decimal point
	bool after_point = false; // True if we are after the decimal point
	if (*s == '.') {
		after_point = true;
		for (++s; *s == '0'; ++s) {
			++n_leading;
		}
	}

	// Read significant digits of the mantissa into a 64-bit integer
	const char* const digits   = s; // Store pointer to start of digits
	uint64_t          frac     = 0; // Fraction value (ignoring decimal point)
	int               n_total  = 0; // Number of decimal digits in fraction
	int               n_before = 0; // Number of digits before decimal point
	int               n_after  = 0; // Number of digits after decimal point
	for (int i = 0; i < uint64_digits10; ++i, ++s) {
		if (is_digit(*s)) {
			frac = (frac * 10) + (unsigned)(*s - '0');
			++n_total;
			n_before += !after_point;
			n_after  += after_point;
		} else if (*s == '.' && !after_point) {
			after_point = true;
		} else {
			break;
		}
	}

	// Skip extra digits
	const int n_used         = MAX(n_total, n_leading ? 1 : 0);
	int       n_extra_before = 0;
	int       n_extra_after  = 0;
	for (;; ++s, ++n_total) {
		if (*s == '.' && !after_point) {
			after_point = true;
		} else if (is_digit(*s)) {
			n_extra_before += !after_point;
			n_extra_after  += after_point;
		} else {
			break;
		}
	}

	// Read exponent from input
	int abs_in_expt  = 0;
	int in_expt_sign = 1;
	if (*s == 'e' || *s == 'E') {
		++s;
		in_expt_sign = read_sign(&s);
		while (is_digit(*s)) {
			abs_in_expt = (abs_in_expt * 10) + (*s++ - '0');
		}
	}

	// Calculate output exponents
	const int in_expt     = in_expt_sign * abs_in_expt;
	const int frac_expt   = n_extra_before - n_after - n_leading + in_expt;
	const int digits_expt = in_expt - n_after - n_extra_after - n_leading;

	const SerdParsedDouble result = {sign,
	                                 digits_expt,
	                                 digits,
	                                 frac,
	                                 frac_expt,
	                                 n_used,
	                                 (size_t)(s - str)};

	return result;
}

static uint64_t
normalize(SerdSoftFloat* value, const uint64_t error)
{
	const int original_e = value->e;

	*value = soft_float_normalize(*value);

	return error << (original_e - value->e);
}

/**
   Return the error added by floating point multiplication.

   Should be l + r + l*r/(2^64) + 0.5, but we short the denominator to 63 due
   to lack of precision, which effectively rounds up.
*/
static inline uint64_t
product_error(const uint64_t lerror,
              const uint64_t rerror,
              const uint64_t half_ulp)
{
	return lerror + rerror + ((lerror * rerror) >> 63) + half_ulp;
}

/**
   Guess the binary floating point value for decimal input.

   @param significand Significand from the input.
   @param expt10 Decimal exponent from the input.
   @param n_digits Number of decimal digits in the significand.
   @param[out] guess Either the exact number, or its predecessor.
   @return True if `guess` is correct.
*/
static bool
sftod(const uint64_t       significand,
      const int            expt10,
      const int            n_digits,
      SerdSoftFloat* const guess)
{
	assert(sizeof(guess->f) == sizeof(significand));
	assert(expt10 <= max_dec_expt);
	assert(expt10 >= min_dec_expt);

	/* The general idea here is to try and find a power of 10 that we can
	   multiply by the significand to get the number.  We get one from the
	   cache which is possibly too small, then multiply by another power of 10
	   to make up the difference if necessary.  For example, with a target
	   power of 10^70, if we get 10^68 from the cache, then we multiply again
	   by 10^2.  This, as well as normalization, accumulates error, which is
	   tracked throughout to know if we got the precise number. */

	// Use a common denominator of 2^3 to avoid fractions
	static const int      lg_denom = 3;
	static const uint64_t denom    = 1 << 3;
	static const uint64_t half_ulp = 4;

	// Start out with just the significand, and no error
	SerdSoftFloat input = {significand, 0};
	uint64_t      error = normalize(&input, 0);

	// Get a power of 10 that takes us most of the way without overshooting
	int           cached_expt10;
	SerdSoftFloat pow10 = soft_float_pow10_under(expt10, &cached_expt10);

	// Get an exact fixup power if necessary
	const int d_expt10 = expt10 - cached_expt10;
	if (d_expt10) {
		input = soft_float_multiply(input, soft_float_exact_pow10(d_expt10));
		if (d_expt10 > uint64_digits10 - n_digits) {
			error += half_ulp; // Product does not fit in an integer
		}
	}

	// Multiply the significand by the power, normalize, and update the error
	input = soft_float_multiply(input, pow10);
	error = normalize(&input, product_error(error, half_ulp, half_ulp));

	// Get the effective number of significant bits from the order of magnitude
	const int magnitude          = 64 + input.e;
	const int real_magnitude     = magnitude - dbl_subnormal_expt;
	const int n_significant_bits = CLAMP(real_magnitude, 0, DBL_MANT_DIG);

	// Calculate the number of "extra" bits of precision we have
	int n_extra_bits = 64 - n_significant_bits;
	if (n_extra_bits + lg_denom >= 64) {
		// Very small subnormal where extra * denom does not fit in an integer
		// Shift right (and accumulate some more error) to compensate
		const int amount = (n_extra_bits + lg_denom) - 63;

		input.f >>= amount;
		input.e += amount;
		error = product_error((error >> amount) + 1, half_ulp, half_ulp);
		n_extra_bits -= amount;
	}

	// Calculate boundaries for the extra bits (with the common denominator)
	assert(n_extra_bits < 64);
	const uint64_t extra_mask = (1ull << n_extra_bits) - 1;
	const uint64_t extra_bits = (input.f & extra_mask) * denom;
	const uint64_t middle     = (1ull << (n_extra_bits - 1)) * denom;
	const uint64_t low        = middle - error;
	const uint64_t high       = middle + error;

	// Round to nearest representable double
	guess->f = (input.f >> n_extra_bits) + (extra_bits >= high);
	guess->e = input.e + n_extra_bits;

	// Too inaccurate if the extra bits are within the error around the middle
	return extra_bits <= low || extra_bits >= high;
}

static int
compare_buffer(const char* buf, const int expt, const SerdSoftFloat upper)
{
	SerdBigint buf_bigint;
	serd_bigint_set_decimal_string(&buf_bigint, buf);

	SerdBigint upper_bigint;
	serd_bigint_set_u64(&upper_bigint, upper.f);

	if (expt >= 0) {
		serd_bigint_multiply_pow10(&buf_bigint, (unsigned)expt);
	} else {
		serd_bigint_multiply_pow10(&upper_bigint, (unsigned)-expt);
	}

	if (upper.e > 0) {
		serd_bigint_shift_left(&upper_bigint, (unsigned)upper.e);
	} else {
		serd_bigint_shift_left(&buf_bigint, (unsigned)-upper.e);
	}

	return serd_bigint_compare(&buf_bigint, &upper_bigint);
}

double
serd_strtod(const char* const str, size_t* const end)
{
#define SET_END(index) do { if (end) { *end = (size_t)(index); } } while (0)

	static const int n_exact_pow10        = sizeof(POW10) / sizeof(POW10[0]);
	static const int max_exact_int_digits = 15;   // Digits that fit exactly
	static const int max_decimal_power    = 309;  // Max finite power
	static const int min_decimal_power    = -324; // Min non-zero power

	// Point s at the first non-whitespace character
	const char* s = str;
	while (is_space(*s)) {
		++s;
	}

	// Handle non-numeric special cases
	if (!strcmp(s, "NaN")) {
		SET_END(s - str + 3);
		return (double)NAN;
	} else if (!strcmp(s, "-INF")) {
		SET_END(s - str + 4);
		return (double)-INFINITY;
	} else if (!strcmp(s, "INF")) {
		SET_END(s - str + 3);
		return (double)INFINITY;
	} else if (!strcmp(s, "+INF")) {
		SET_END(s - str + 4);
		return (double)INFINITY;
	} else if (*s != '+' && *s != '-' && *s != '.' && !is_digit(*s)) {
		SET_END(s - str);
		return (double)NAN;
	}

	const SerdParsedDouble in = serd_parse_double(s);
	SET_END(in.end);
#undef SET_END

	if (in.n_digits == 0) {
		return (double)NAN;
	}

	const int expt         = in.frac_expt;
	const int result_power = in.n_digits + expt;

	// Return early for simple exact cases
	if (result_power > max_decimal_power) {
		return (double)in.sign * (double)INFINITY;
	} else if (result_power < min_decimal_power) {
		return in.sign * 0.0;
	} else if (in.n_digits < max_exact_int_digits) {
		if (expt < 0 && -expt < n_exact_pow10) {
			return in.sign * (in.frac / (double)POW10[-expt]);
		} else if (expt >= 0 && expt < n_exact_pow10) {
			return in.sign * (in.frac * (double)POW10[expt]);
		}
	}

	// Try to guess the number using only soft floating point (fast path)
	SerdSoftFloat guess = {0, 0};
	const bool    exact = sftod(in.frac, expt, in.n_digits, &guess);
	const double  g     = soft_float_to_double(guess);
	if (exact) {
		return (double)in.sign * g;
	}

	// Not sure, guess is either the number or its predecessor (rare slow path)
	// Compare it with the buffer using bigints to find out which
	const SerdSoftFloat upper = {guess.f * 2 + 1, guess.e - 1};
	const int           cmp   = compare_buffer(in.digits, in.digits_expt, upper);
	if (cmp < 0) {
		return in.sign * g;
	} else if (cmp > 0) {
		return in.sign * nextafter(g, (double)INFINITY);
	} else if ((guess.f & 1) == 0) {
		return in.sign * g; // Round towards even
	} else {
		return in.sign * nextafter(g, (double)INFINITY); // Round odd up
	}
}
