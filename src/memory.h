// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MEMORY_H
#define SERD_SRC_MEMORY_H

#include "serd/world.h"
#include "zix/allocator.h"

#include <stddef.h>

#define SERD_PAGE_SIZE 4096U

// Allocator convenience wrappers that use the world allocator

static inline void*
serd_wmalloc(const SerdWorld* const world, const size_t size)
{
  return zix_malloc(serd_world_allocator(world), size);
}

static inline void*
serd_wcalloc(const SerdWorld* const world,
             const size_t           nmemb,
             const size_t           size)
{
  return zix_calloc(serd_world_allocator(world), nmemb, size);
}

static inline void*
serd_wrealloc(const SerdWorld* const world, void* const ptr, const size_t size)
{
  return zix_realloc(serd_world_allocator(world), ptr, size);
}

static inline void
serd_wfree(const SerdWorld* const world, void* const ptr)
{
  zix_free(serd_world_allocator(world), ptr);
}

static inline void*
serd_waligned_alloc(const SerdWorld* const world,
                    const size_t           alignment,
                    const size_t           size)
{
  return zix_aligned_alloc(serd_world_allocator(world), alignment, size);
}

static inline void
serd_waligned_free(const SerdWorld* const world, void* const ptr)
{
  zix_aligned_free(serd_world_allocator(world), ptr);
}

#endif // SERD_SRC_MEMORY_H
