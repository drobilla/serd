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

#include "../src/decimal.h"

#include <assert.h>

static void
test_count_digits(void)
{
	assert(1 == serd_count_digits(0));
	assert(1 == serd_count_digits(1));
	assert(1 == serd_count_digits(9));
	assert(2 == serd_count_digits(10));
	assert(2 == serd_count_digits(99ull));
	assert(3 == serd_count_digits(999ull));
	assert(4 == serd_count_digits(9999ull));
	assert(5 == serd_count_digits(99999ull));
	assert(6 == serd_count_digits(999999ull));
	assert(7 == serd_count_digits(9999999ull));
	assert(8 == serd_count_digits(99999999ull));
	assert(9 == serd_count_digits(999999999ull));
	assert(10 == serd_count_digits(9999999999ull));
	assert(11 == serd_count_digits(99999999999ull));
	assert(12 == serd_count_digits(999999999999ull));
	assert(13 == serd_count_digits(9999999999999ull));
	assert(14 == serd_count_digits(99999999999999ull));
	assert(15 == serd_count_digits(999999999999999ull));
	assert(16 == serd_count_digits(9999999999999999ull));
	assert(17 == serd_count_digits(99999999999999999ull));
	assert(18 == serd_count_digits(999999999999999999ull));
	assert(19 == serd_count_digits(9999999999999999999ull));
	assert(20 == serd_count_digits(18446744073709551615ull));
}

int
main(void)
{
	test_count_digits();
}
