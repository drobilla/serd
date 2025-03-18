// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STACK_H
#define SERD_SRC_STACK_H

#include <zix/allocator.h>

#include <assert.h>
#include <stddef.h>

/// A dynamic stack in memory
typedef struct {
  char*  buf;      ///< Stack memory
  size_t buf_size; ///< Allocated size of buf (>= size)
  size_t size;     ///< Conceptual size of stack in buf
} SerdStack;

static inline SerdStack
serd_stack_new(ZixAllocator* const allocator, const size_t size)
{
  SerdStack stack;
  stack.buf      = (char*)zix_calloc(allocator, 1U, size);
  stack.buf_size = size;
  stack.size     = 0;
  return stack;
}

static inline void
serd_stack_free(ZixAllocator* const allocator, SerdStack* const stack)
{
  zix_free(allocator, stack->buf);
  stack->buf      = NULL;
  stack->buf_size = 0;
  stack->size     = 0;
}

static inline void*
serd_stack_push(SerdStack* const stack, const size_t n_bytes)
{
  const size_t new_size = stack->size + n_bytes;
  if (stack->buf_size < new_size) {
    return NULL;
  }

  char* const ret = (stack->buf + stack->size);
  stack->size     = new_size;
  return ret;
}

static inline void
serd_stack_pop(SerdStack* const stack, const size_t n_bytes)
{
  assert(stack->size >= n_bytes);
  stack->size -= n_bytes;
}

static inline void
serd_stack_pop_to(SerdStack* stack, size_t n_bytes)
{
  assert(stack->size >= n_bytes);
  stack->size = n_bytes;
}

#endif // SERD_SRC_STACK_H
