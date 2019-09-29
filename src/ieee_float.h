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

#ifndef SERD_IEEE_FLOAT_H
#define SERD_IEEE_FLOAT_H

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const int      dbl_physical_mant_dig = DBL_MANT_DIG - 1;
static const uint64_t dbl_mant_mask         = 0x000FFFFFFFFFFFFFull;
static const uint64_t dbl_expt_mask         = 0x7FF0000000000000ul;
static const uint64_t dbl_hidden_bit        = 0x0010000000000000ul;
static const int      dbl_expt_bias         = 0x3FF + DBL_MANT_DIG - 1;
static const int      dbl_subnormal_expt    = -0x3FF - DBL_MANT_DIG + 2;

/// Return the raw representation of a double
static inline uint64_t
double_to_rep(const double d)
{
	uint64_t rep;
	memcpy(&rep, &d, sizeof(rep));
	return rep;
}

/// Return true if the lower boundary is closer than the upper boundary
static inline bool
double_lower_boundary_is_closer(const double d)
{
	const uint64_t rep          = double_to_rep(d);
	const uint64_t mant         = rep & dbl_mant_mask;
	const uint64_t expt         = rep & dbl_expt_mask;
	const bool     is_subnormal = expt == 0;

	// True when f = 2^(p-1) (except for the smallest normal)
	return !is_subnormal && mant == 0;
}

#endif // SERD_IEEE_FLOAT_H
