// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "failing_allocator.h"

#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool
attempt(SerdFailingAllocator* const allocator)
{
  ++allocator->n_allocations;

  if (!allocator->n_remaining) {
    return false;
  }

  --allocator->n_remaining;
  return true;
}

ZIX_MALLOC_FUNC static void*
serd_failing_malloc(ZixAllocator* const allocator, const size_t size)
{
  SerdFailingAllocator* const state = (SerdFailingAllocator*)allocator;
  ZixAllocator* const         base  = zix_default_allocator();

  return attempt(state) ? base->malloc(base, size) : NULL;
}

ZIX_MALLOC_FUNC static void*
serd_failing_calloc(ZixAllocator* const allocator,
                    const size_t        nmemb,
                    const size_t        size)
{
  SerdFailingAllocator* const state = (SerdFailingAllocator*)allocator;
  ZixAllocator* const         base  = zix_default_allocator();

  return attempt(state) ? base->calloc(base, nmemb, size) : NULL;
}

static void*
serd_failing_realloc(ZixAllocator* const allocator,
                     void* const         ptr,
                     const size_t        size)
{
  SerdFailingAllocator* const state = (SerdFailingAllocator*)allocator;
  ZixAllocator* const         base  = zix_default_allocator();

  return attempt(state) ? base->realloc(base, ptr, size) : NULL;
}

static void
serd_failing_free(ZixAllocator* const allocator, void* const ptr)
{
  (void)allocator;

  ZixAllocator* const base = zix_default_allocator();

  base->free(base, ptr);
}

ZIX_MALLOC_FUNC static void*
serd_failing_aligned_alloc(ZixAllocator* const allocator,
                           const size_t        alignment,
                           const size_t        size)
{
  SerdFailingAllocator* const state = (SerdFailingAllocator*)allocator;
  ZixAllocator* const         base  = zix_default_allocator();

  return attempt(state) ? base->aligned_alloc(base, alignment, size) : NULL;
}

static void
serd_failing_aligned_free(ZixAllocator* const allocator, void* const ptr)
{
  (void)allocator;

  ZixAllocator* const base = zix_default_allocator();

  base->aligned_free(base, ptr);
}

ZIX_CONST_FUNC SerdFailingAllocator
serd_failing_allocator(void)
{
  SerdFailingAllocator failing_allocator = {
    {
      serd_failing_malloc,
      serd_failing_calloc,
      serd_failing_realloc,
      serd_failing_free,
      serd_failing_aligned_alloc,
      serd_failing_aligned_free,
    },
    0,
    SIZE_MAX,
  };

  return failing_allocator;
}
