/* Serd, an RDF serialisation library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 *
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SERD_STACK_H
#define SERD_STACK_H

#include <assert.h>

#include "serd/serd.h"

/** An offset to start the stack at.  Note 0 is reserved for NULL. */
#define SERD_STACK_BOTTOM sizeof(void*)

typedef struct {
	uint8_t* buf;       ///< Stack memory
	size_t   buf_size;  ///< Allocated size of buf (>= size)
	size_t   size;      ///< Conceptual size of stack in buf
} SerdStack;

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

#endif  // SERD_STACK_H
