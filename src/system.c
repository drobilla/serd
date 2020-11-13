// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "system.h"

#include "serd_config.h"
#include "serd_internal.h"

#ifdef _WIN32
#  include <malloc.h>
#endif

#include <stdio.h>
#include <stdlib.h>

void*
serd_malloc_aligned(const size_t alignment, const size_t size)
{
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

void*
serd_allocate_buffer(const size_t size)
{
  return serd_malloc_aligned(SERD_PAGE_SIZE, size);
}

void
serd_free_aligned(void* const ptr)
{
#ifdef _WIN32
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}
