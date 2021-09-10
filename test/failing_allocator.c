/*
  Copyright 2021 David Robillard <d@drobilla.net>

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

#include "failing_allocator.h"

#include "serd/serd.h"

#include <stdbool.h>
#include <stddef.h>

static bool
attempt(SerdFailingAllocatorState* const state)
{
  ++state->n_allocations;

  if (!state->n_remaining) {
    return false;
  }

  --state->n_remaining;
  return true;
}

static void*
serd_failing_malloc(SerdAllocatorHandle* const handle, const size_t size)
{
  SerdFailingAllocatorState* const state = (SerdFailingAllocatorState*)handle;
  const SerdAllocator* const       base  = serd_default_allocator();

  return attempt(state) ? base->malloc(base->handle, size) : NULL;
}

static void*
serd_failing_calloc(SerdAllocatorHandle* const handle,
                    const size_t               nmemb,
                    const size_t               size)
{
  SerdFailingAllocatorState* const state = (SerdFailingAllocatorState*)handle;
  const SerdAllocator* const       base  = serd_default_allocator();

  return attempt(state) ? base->calloc(base->handle, nmemb, size) : NULL;
}

static void*
serd_failing_realloc(SerdAllocatorHandle* const handle,
                     void* const                ptr,
                     const size_t               size)
{
  SerdFailingAllocatorState* const state = (SerdFailingAllocatorState*)handle;
  const SerdAllocator* const       base  = serd_default_allocator();

  return attempt(state) ? base->realloc(base->handle, ptr, size) : NULL;
}

static void
serd_failing_free(SerdAllocatorHandle* const handle, void* const ptr)
{
  (void)handle;

  const SerdAllocator* const base = serd_default_allocator();

  base->free(base->handle, ptr);
}

static void*
serd_failing_aligned_alloc(SerdAllocatorHandle* const handle,
                           const size_t               alignment,
                           const size_t               size)
{
  SerdFailingAllocatorState* const state = (SerdFailingAllocatorState*)handle;
  const SerdAllocator* const       base  = serd_default_allocator();

  return attempt(state) ? base->aligned_alloc(base->handle, alignment, size)
                        : NULL;
}

static void
serd_failing_aligned_free(SerdAllocatorHandle* const handle, void* const ptr)
{
  (void)handle;

  const SerdAllocator* const base = serd_default_allocator();

  base->aligned_free(base->handle, ptr);
}

SerdAllocator
serd_failing_allocator(SerdFailingAllocatorState* const state)
{
  const SerdAllocator failing_allocator = {
    state,
    serd_failing_malloc,
    serd_failing_calloc,
    serd_failing_realloc,
    serd_failing_free,
    serd_failing_aligned_alloc,
    serd_failing_aligned_free,
  };

  return failing_allocator;
}
