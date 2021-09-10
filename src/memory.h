/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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

#ifndef SERD_MEMORY_H
#define SERD_MEMORY_H

#include "serd/serd.h"

#include <stddef.h>
#include <string.h>

// Allocator convenience wrappers that fall back to the default for NULL

static inline void*
serd_amalloc(const SerdAllocator* const allocator, const size_t size)
{
  const SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->malloc(actual->handle, size);
}

static inline void*
serd_acalloc(const SerdAllocator* const allocator,
             const size_t               nmemb,
             const size_t               size)
{
  const SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->calloc(actual->handle, nmemb, size);
}

static inline void*
serd_arealloc(const SerdAllocator* const allocator,
              void* const                ptr,
              const size_t               size)
{
  const SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->realloc(actual->handle, ptr, size);
}

static inline void
serd_afree(const SerdAllocator* const allocator, void* const ptr)
{
  const SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  actual->free(actual->handle, ptr);
}

static inline void*
serd_aaligned_alloc(const SerdAllocator* const allocator,
                    const size_t               alignment,
                    const size_t               size)
{
  const SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->aligned_alloc(actual->handle, alignment, size);
}

static inline void*
serd_aaligned_calloc(const SerdAllocator* const allocator,
                     const size_t               alignment,
                     const size_t               size)
{
  void* const ptr = serd_aaligned_alloc(allocator, alignment, size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static inline void
serd_aaligned_free(const SerdAllocator* const allocator, void* const ptr)
{
  const SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  actual->aligned_free(actual->handle, ptr);
}

// World convenience wrappers

static inline void*
serd_wmalloc(const SerdWorld* const world, const size_t size)
{
  return serd_amalloc(serd_world_allocator(world), size);
}

static inline void*
serd_wcalloc(const SerdWorld* const world,
             const size_t           nmemb,
             const size_t           size)
{
  return serd_acalloc(serd_world_allocator(world), nmemb, size);
}

static inline void*
serd_wrealloc(const SerdWorld* const world, void* const ptr, const size_t size)
{
  return serd_arealloc(serd_world_allocator(world), ptr, size);
}

static inline void
serd_wfree(const SerdWorld* const world, void* const ptr)
{
  serd_afree(serd_world_allocator(world), ptr);
}

static inline void*
serd_waligned_alloc(const SerdWorld* const world,
                    const size_t           alignment,
                    const size_t           size)
{
  return serd_aaligned_alloc(serd_world_allocator(world), alignment, size);
}

static inline void
serd_waligned_free(const SerdWorld* const world, void* const ptr)
{
  serd_aaligned_free(serd_world_allocator(world), ptr);
}

#endif // SERD_MEMORY_H
