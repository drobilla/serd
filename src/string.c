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

#include "serd_internal.h"

#include <math.h>

SERD_API
const uint8_t*
serd_strerror(SerdStatus st)
{
	switch (st) {
	case SERD_SUCCESS:        return (const uint8_t*)"Success";
	case SERD_FAILURE:        return (const uint8_t*)"Non-fatal failure";
	case SERD_ERR_UNKNOWN:    return (const uint8_t*)"Unknown error";
	case SERD_ERR_BAD_SYNTAX: return (const uint8_t*)"Invalid syntax";
	case SERD_ERR_BAD_ARG:    return (const uint8_t*)"Invalid argument";
	case SERD_ERR_NOT_FOUND:  return (const uint8_t*)"Not found";
	}
	return (const uint8_t*)"Success";  // never reached
}

SERD_API
size_t
serd_strlen(const uint8_t* str, size_t* n_bytes, SerdNodeFlags* flags)
{
	size_t n_chars = 0;
	size_t i       = 0;
	*flags         = 0;
	for (; str[i]; ++i) {
		if ((str[i] & 0xC0) != 0x80) {
			// Does not start with `10', start of a new character
			++n_chars;
			switch (str[i]) {
			case '\r':
			case '\n':
				*flags |= SERD_HAS_NEWLINE;
				break;
			case '"':
				*flags |= SERD_HAS_QUOTE;
			}
		}
	}
	if (n_bytes) {
		*n_bytes = i;
	}
	return n_chars;
}

static inline double
read_sign(const char** sptr)
{
	double sign = 1.0;
	switch (**sptr) {
	case '-': sign = -1.0;
	case '+': ++(*sptr);
	default:  return sign;
	}
}

SERD_API
double
serd_strtod(const char* str, char** endptr)
{
	double result = 0.0;

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

	*endptr = (char*)s;
	return result * sign;
}
