// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STACK_H
#define SERD_SRC_STACK_H

#include "memory.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/** A dynamic stack in memory. */
typedef struct {
  char*  buf;      ///< Stack memory
  size_t buf_size; ///< Allocated size of buf (>= size)
  size_t size;     ///< Conceptual size of stack in buf
} SerdStack;

static inline SerdStack
serd_stack_new(ZixAllocator* const allocator, size_t size, size_t align)
{
  const size_t aligned_size = (size + (align - 1)) / align * align;

  SerdStack stack;
  stack.buf      = (char*)zix_aligned_alloc(allocator, align, aligned_size);
  stack.buf_size = size;
  stack.size     = align; // 0 is reserved for null

  if (stack.buf) {
    memset(stack.buf, 0, size);
  }

  return stack;
}

static inline void
serd_stack_free(ZixAllocator* const allocator, SerdStack* stack)
{
  zix_aligned_free(allocator, stack->buf);
  stack->buf      = NULL;
  stack->buf_size = 0;
  stack->size     = 0;
}

static inline void*
serd_stack_push(SerdStack* stack, size_t n_bytes)
{
  const size_t old_size = stack->size;
  const size_t new_size = old_size + n_bytes;
  if (stack->buf_size < new_size) {
    return NULL;
  }

  stack->size = new_size;
  return stack->buf + old_size;
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
serd_stack_push_pad(SerdStack* stack, size_t align)
{
  // Push padding if necessary
  const size_t leftovers = stack->size % align;
  if (leftovers) {
    const size_t pad     = align - leftovers;
    void* const  padding = serd_stack_push(stack, pad);
    if (!padding) {
      return NULL;
    }
    memset(padding, 0, pad);
    return padding;
  }

  return stack->buf + stack->size;
}

#endif // SERD_SRC_STACK_H
