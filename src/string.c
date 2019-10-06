/*
  Copyright 2011-2016 David Robillard <http://drobilla.net>

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

#include "int_math.h"
#include "string_utils.h"

#include "serd/serd.h"

#include <math.h>
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


double
serd_strtod(const char* str, size_t* end)
{
#define SET_END(index) if (end) { *end = (size_t)(index); }

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

	return in.sign * (in.frac * pow(10, in.frac_expt));
}
