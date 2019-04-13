/*
  Copyright 2011-2018 David Robillard <http://drobilla.net>

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

#ifndef SERD_STACK_H
#define SERD_STACK_H

#include "system.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** An offset to start the stack at. Note 0 is reserved for NULL. */
#define SERD_STACK_BOTTOM sizeof(void*)

/** A dynamic stack in memory. */
typedef struct {
	char*    buf;       ///< Stack memory
	size_t   buf_size;  ///< Allocated size of buf (>= size)
	size_t   size;      ///< Conceptual size of stack in buf
} SerdStack;

static inline SerdStack
serd_stack_new(size_t size)
{
	SerdStack stack;
	stack.buf       = (char*)serd_calloc_aligned(size, sizeof(SerdNode));
	stack.buf_size  = size;
	stack.size      = SERD_STACK_BOTTOM;
	return stack;
}

static inline bool
serd_stack_is_empty(const SerdStack* stack)
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

static inline void*
serd_stack_push(SerdStack* stack, size_t n_bytes)
{
	const size_t new_size = stack->size + n_bytes;
	if (stack->buf_size < new_size) {
		return NULL;
	}
	char* const ret = (stack->buf + stack->size);
	stack->size = new_size;
	return ret;
}

static inline void
serd_stack_pop(SerdStack* stack, size_t n_bytes)
{
	assert(stack->size >= n_bytes);
	stack->size -= n_bytes;
}

static inline void
serd_stack_pop_to(SerdStack* stack, size_t n_bytes)
{
	assert(stack->size >= n_bytes);
	memset(stack->buf + n_bytes, 0, stack->size - n_bytes);
	stack->size = n_bytes;
}

static inline void*
serd_stack_push_aligned(SerdStack* stack, size_t n_bytes, size_t align)
{
	// Push padding if necessary
	const size_t pad = align - stack->size % align;
	if (pad > 0) {
		void* padding = serd_stack_push(stack, pad);
		if (!padding) {
			return NULL;
		}
		memset(padding, 0, pad);
	}

	// Push requested space at aligned location
	return serd_stack_push(stack, n_bytes);
}

#endif  // SERD_STACK_H
