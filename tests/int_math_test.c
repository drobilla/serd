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

#undef NDEBUG

#include "../src/int_math.h"

#include <assert.h>

static void
test_clz32(void)
{
	for (unsigned i = 0; i < 32; ++i) {
		assert(serd_clz32(1u << i) == 32u - i - 1u);
	}
}

static void
test_clz64(void)
{
	for (unsigned i = 0; i < 64; ++i) {
		assert(serd_clz64(1ull << i) == 64u - i - 1u);
	}
}

static void
test_ilog2(void)
{
	for (unsigned i = 0; i < 64; ++i) {
		assert(serd_ilog2(1ull << i) == i);
	}
}

static void
test_ilog10(void)
{
	uint64_t power = 1;
	for (unsigned i = 0; i < 20; ++i, power *= 10) {
		assert(serd_ilog10(power) == i);
	}
}

int
main(void)
{
	test_clz32();
	test_clz64();
	test_ilog2();
	test_ilog10();
	return 0;
}
