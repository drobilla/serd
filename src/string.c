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

#include "serd/serd.h"
#include "string_utils.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

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

static inline double
read_sign(const char** sptr)
{
	double sign = 1.0;
	switch (**sptr) {
	case '-':
		sign = -1.0;
		// fallthru
	case '+':
		++(*sptr);
		// fallthru
	default:
		return sign;
	}
}

double
serd_strtod(const char* str, size_t* end)
{
	double result = 0.0;

#define SET_END(index) if (end) { *end = index; }

	if (!strcmp(str, "NaN")) {
		SET_END(3);
		return NAN;
	} else if (!strcmp(str, "-INF")) {
		SET_END(4);
		return -INFINITY;
	} else if (!strcmp(str, "INF")) {
		SET_END(3);
		return INFINITY;
	}

	// Point s at the first non-whitespace character
	const char* s = str;
	while (is_space(*s)) { ++s; }

	// Read leading sign if necessary
	const double sign = read_sign(&s);

	// Parse integer part
	for (; is_digit(*s); ++s) {
		result = (result * 10.0) + (*s - '0');
	}

	// Parse fractional part
	if (*s == '.') {
		double denom = 10.0;
		for (++s; is_digit(*s); ++s) {
			result += (*s - '0') / denom;
			denom *= 10.0;
		}
	}

	// Parse exponent
	if (*s == 'e' || *s == 'E') {
		++s;
		double expt      = 0.0;
		double expt_sign = read_sign(&s);
		for (; is_digit(*s); ++s) {
			expt = (expt * 10.0) + (*s - '0');
		}
		result *= pow(10, expt * expt_sign);
	}

	SET_END(s - str);
	return result * sign;
}
