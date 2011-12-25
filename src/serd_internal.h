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

#ifndef SERD_INTERNAL_H
#define SERD_INTERNAL_H

#define _POSIX_C_SOURCE 201112L /* for posix_memalign and posix_fadvise */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"
#include "serd-config.h"

#if defined(HAVE_POSIX_FADVISE) && defined(HAVE_FILENO)
#   include <fcntl.h>
#endif

#define SERD_PAGE_SIZE 4096

#ifndef MIN
#    define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* File and Buffer Utilities */

static inline FILE*
serd_fopen(const char* path, const char* mode)
{
	FILE* fd = fopen((const char*)path, mode);
	if (!fd) {
		fprintf(stderr, "Error opening file %s (%s)\n", path, strerror(errno));
		return NULL;
	}
#if defined(HAVE_POSIX_FADVISE) && defined(HAVE_FILENO)
	posix_fadvise(fileno(fd), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
	return fd;
}

static inline void*
serd_bufalloc(size_t size)
{
#ifdef HAVE_POSIX_MEMALIGN
	void* ptr;
	posix_memalign(&ptr, SERD_PAGE_SIZE, size);
	return ptr;
#else
	return malloc(size);
#endif
}

/* Stack */

/** A dynamic stack in memory. */
typedef struct {
	uint8_t* buf;       ///< Stack memory
	size_t   buf_size;  ///< Allocated size of buf (>= size)
	size_t   size;      ///< Conceptual size of stack in buf
} SerdStack;

/** An offset to start the stack at. Note 0 is reserved for NULL. */
#define SERD_STACK_BOTTOM sizeof(void*)

static inline SerdStack
serd_stack_new(size_t size)
{
	SerdStack stack;
	stack.buf       = malloc(size);
	stack.buf_size  = size;
	stack.size      = SERD_STACK_BOTTOM;
	return stack;
}

static inline bool
serd_stack_is_empty(SerdStack* stack)
{
	return stack->size <= SERD_STACK_BOTTOM;
}

static inline void
serd_stack_free(SerdStack* stack)
{
	free(stack->buf);
	stack->buf      = NULL;
	stack->buf_size = 0;
	stack->size     = 0;
}

static inline uint8_t*
serd_stack_push(SerdStack* stack, size_t n_bytes)
{
	const size_t new_size = stack->size + n_bytes;
	if (stack->buf_size < new_size) {
		stack->buf_size *= 2;
		stack->buf = realloc(stack->buf, stack->buf_size);
	}
	uint8_t* const ret = (stack->buf + stack->size);
	stack->size = new_size;
	return ret;
}

static inline void
serd_stack_pop(SerdStack* stack, size_t n_bytes)
{
	assert(stack->size >= n_bytes);
	stack->size -= n_bytes;
}

/* Bulk Sink */

typedef struct SerdBulkSinkImpl {
	SerdSink sink;
	void*    stream;
	uint8_t* buf;
	size_t   size;
	size_t   block_size;
} SerdBulkSink;

static inline SerdBulkSink
serd_bulk_sink_new(SerdSink sink, void* stream, size_t block_size)
{
	SerdBulkSink bsink;
	bsink.sink       = sink;
	bsink.stream     = stream;
	bsink.size       = 0;
	bsink.block_size = block_size;
	bsink.buf        = serd_bufalloc(block_size);
	return bsink;
}

static inline void
serd_bulk_sink_flush(SerdBulkSink* bsink)
{
	if (bsink->size > 0) {
		bsink->sink(bsink->buf, bsink->size, bsink->stream);
	}
	bsink->size = 0;
}

static inline void
serd_bulk_sink_free(SerdBulkSink* bsink)
{
	serd_bulk_sink_flush(bsink);
	free(bsink->buf);
	bsink->buf = NULL;
}

static inline size_t
serd_bulk_sink_write(const void* buf, size_t len, SerdBulkSink* bsink)
{
	const size_t orig_len = len;
	while (len) {
		const size_t space = bsink->block_size - bsink->size;
		const size_t n     = MIN(space, len);

		// Write as much as possible into the remaining buffer space
		memcpy(bsink->buf + bsink->size, buf, n);
		bsink->size += n;
		buf           = (uint8_t*)buf + n;
		len          -= n;

		// Flush page if buffer is full
		if (bsink->size == bsink->block_size) {
			bsink->sink(bsink->buf, bsink->block_size, bsink->stream);
			bsink->size = 0;
		}
	}
	return orig_len;
}

/* Character utilities */

/** Return true if @a c lies within [min...max] (inclusive) */
static inline bool
in_range(const uint8_t c, const uint8_t min, const uint8_t max)
{
	return (c >= min && c <= max);
}

/** RFC2234: ALPHA := %x41-5A / %x61-7A  ; A-Z / a-z */
static inline bool
is_alpha(const uint8_t c)
{
	return in_range(c, 'A', 'Z') || in_range(c, 'a', 'z');
}

/** RFC2234: DIGIT ::= %x30-39  ; 0-9 */
static inline bool
is_digit(const uint8_t c)
{
	return in_range(c, '0', '9');
}

static inline bool
is_space(const char c)
{
	switch (c) {
	case ' ': case '\f': case '\n': case '\r': case '\t': case '\v':
		return true;
	default:
		return false;
	}
}

#endif  // SERD_INTERNAL_H
