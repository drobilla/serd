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

#ifndef SERD_SYSTEM_H
#define SERD_SYSTEM_H

#include <stdint.h>
#include <stdio.h>

#define SERD_PAGE_SIZE 4096

/** Faster fread-like wrapper for getc. */
static inline size_t
serd_file_read_byte(void* buf, size_t size, size_t nmemb, void* stream)
{
	(void)size;
	(void)nmemb;

	const int c = getc((FILE*)stream);
	if (c == EOF) {
		*((uint8_t*)buf) = 0;
		return 0;
	}
	*((uint8_t*)buf) = (uint8_t)c;
	return 1;
}

/** Allocate a buffer aligned to `alignment` bytes. */
void* serd_malloc_aligned(size_t size, size_t alignment);

/** Allocate a zeroed buffer aligned to `alignment` bytes. */
void* serd_calloc_aligned(size_t size, size_t alignment);

/** Allocate an aligned buffer for I/O. */
void* serd_allocate_buffer(size_t size);

#endif  // SERD_SYSTEM_H
