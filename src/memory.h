// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MEMORY_H
#define SERD_SRC_MEMORY_H

#include "serd/memory.h"
#include "serd/world.h"

#include <stddef.h>
#include <string.h>

// Allocator convenience wrappers that fall back to the default for NULL

/// Convenience wrapper that defers to malloc() if allocator is null
static inline void*
serd_amalloc(SerdAllocator* const allocator, const size_t size)
{
  SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->malloc(actual, size);
}

/// Convenience wrapper that defers to calloc() if allocator is null
static inline void*
serd_acalloc(SerdAllocator* const allocator,
             const size_t         nmemb,
             const size_t         size)
{
  SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->calloc(actual, nmemb, size);
}

/// Convenience wrapper that defers to realloc() if allocator is null
static inline void*
serd_arealloc(SerdAllocator* const allocator,
              void* const          ptr,
              const size_t         size)
{
  SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->realloc(actual, ptr, size);
}

/// Convenience wrapper that defers to free() if allocator is null
static inline void
serd_afree(SerdAllocator* const allocator, void* const ptr)
{
  SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  actual->free(actual, ptr);
}

/// Convenience wrapper that defers to the system allocator if allocator is null
static inline void*
serd_aaligned_alloc(SerdAllocator* const allocator,
                    const size_t         alignment,
                    const size_t         size)
{
  SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  return actual->aligned_alloc(actual, alignment, size);
}

/// Convenience wrapper for serd_aaligned_alloc that zeros memory
static inline void*
serd_aaligned_calloc(SerdAllocator* const allocator,
                     const size_t         alignment,
                     const size_t         size)
{
  void* const ptr = serd_aaligned_alloc(allocator, alignment, size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

/// Convenience wrapper that defers to the system allocator if allocator is null
static inline void
serd_aaligned_free(SerdAllocator* const allocator, void* const ptr)
{
  SerdAllocator* const actual =
    allocator ? allocator : serd_default_allocator();

  actual->aligned_free(actual, ptr);
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

#endif // SERD_SRC_MEMORY_H
