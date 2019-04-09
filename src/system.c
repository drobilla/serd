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

#define _POSIX_C_SOURCE 200809L /* for posix_memalign */

#include "system.h"

#include "serd_config.h"

#include <stdlib.h>
#include <string.h>

void*
serd_malloc_aligned(size_t size, size_t alignment)
{
#ifdef HAVE_POSIX_MEMALIGN
	void*     ptr = NULL;
	const int ret = posix_memalign(&ptr, alignment, size);
	return ret ? NULL : ptr;
#else
	return malloc(size);
#endif
}

void*
serd_calloc_aligned(size_t size, size_t alignment)
{
#ifdef HAVE_POSIX_MEMALIGN
	void* ptr = serd_malloc_aligned(size, alignment);
	if (ptr) {
		memset(ptr, 0, size);
	}
	return ptr;
#else
	return calloc(1, size);
#endif
}

void*
serd_allocate_buffer(size_t size)
{
	return serd_malloc_aligned(size, SERD_PAGE_SIZE);
}
