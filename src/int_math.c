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

#include "int_math.h"

#include <assert.h>

unsigned
serd_clz32(const uint32_t i)
{
	assert(i != 0);

#ifdef HAVE_BUILTIN_CLZ
	return (unsigned)__builtin_clz(i);
#else
	unsigned n    = 32u;
	uint32_t bits = i;
	for (unsigned s = 16; s > 0; s >>= 1) {
		const uint32_t left = bits >> s;
		if (left) {
			n -= s;
			bits = left;
		}
	}
	return n - bits;
#endif
}

unsigned
serd_clz64(const uint64_t i)
{
	assert(i != 0);

#ifdef HAVE_BUILTIN_CLZLL
	return (unsigned)__builtin_clzll(i);
#else
	return i & 0xFFFFFFFF00000000 ? serd_clz32(i >> 32)
	                              : 32 + serd_clz32(i & 0xFFFFFFFF);
#endif
}

uint64_t
serd_ilog2(const uint64_t i)
{
	assert(i != 0);
	return (64 - serd_clz64(i | 1)) - 1;
}

uint64_t
serd_ilog10(const uint64_t i)
{
	// See https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
	const uint64_t log2 = serd_ilog2(i);
	const uint64_t t    = (log2 + 1) * 1233 >> 12;

	return t - (i < POW10[t]) + (i == 0);
}
