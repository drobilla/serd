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

#include "memory.h"

#include "serd_config.h"

#include "serd/serd.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN 1
#  include <malloc.h>
#  include <windows.h>
#endif

#include <stddef.h>
#include <stdlib.h>

static void*
serd_default_malloc(SerdAllocatorHandle* const handle, const size_t size)
{
  (void)handle;
  return malloc(size);
}

static void*
serd_default_calloc(SerdAllocatorHandle* const handle,
                    const size_t               nmemb,
                    const size_t               size)
{
  (void)handle;
  return calloc(nmemb, size);
}

static void*
serd_default_realloc(SerdAllocatorHandle* const handle,
                     void* const                ptr,
                     const size_t               size)
{
  (void)handle;
  return realloc(ptr, size);
}

static void
serd_default_free(SerdAllocatorHandle* const handle, void* const ptr)
{
  (void)handle;
  free(ptr);
}

static void*
serd_default_aligned_alloc(SerdAllocatorHandle* const handle,
                           const size_t               alignment,
                           const size_t               size)
{
  (void)handle;

#if defined(_WIN32)
  return _aligned_malloc(size, alignment);
#elif USE_POSIX_MEMALIGN
  void*     ptr = NULL;
  const int ret = posix_memalign(&ptr, alignment, size);
  return ret ? NULL : ptr;
#else
  (void)alignment;
  return malloc(size);
#endif
}

static void
serd_default_aligned_free(SerdAllocatorHandle* const handle, void* const ptr)
{
  (void)handle;

#ifdef _WIN32
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

const SerdAllocator*
serd_default_allocator(void)
{
  static const SerdAllocator default_allocator = {
    NULL,
    serd_default_malloc,
    serd_default_calloc,
    serd_default_realloc,
    serd_default_free,
    serd_default_aligned_alloc,
    serd_default_aligned_free,
  };

  return &default_allocator;
}
