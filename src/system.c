// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "system.h"

#include "serd_config.h"

#include "serd/string.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN 1
#  include <malloc.h>
#  include <windows.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
serd_system_strerror(const int errnum, char* const buf, const size_t buflen)
{
#if USE_STRERROR_R
  return strerror_r(errnum, buf, buflen);

#else // Not thread-safe, but... oh well?
  const char* const message = strerror(errnum);

  strncpy(buf, message, buflen);
  return 0;
#endif
}

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
serd_calloc_aligned(const size_t alignment, const size_t size)
{
#if defined(_WIN32) || defined(USE_POSIX_MEMALIGN)
  void* const ptr = serd_malloc_aligned(alignment, size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
#else
  (void)alignment;
  return calloc(1, size);
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

char*
serd_canonical_path(const char* const path)
{
  assert(path);

#ifdef _WIN32
  const DWORD size = GetFullPathName(path, 0, NULL, NULL);
  if (size == 0) {
    return NULL;
  }

  char* const out = (char*)calloc(size, 1);
  const DWORD ret = GetFullPathName(path, MAX_PATH, out, NULL);
  if (ret == 0 || ret >= size) {
    free(out);
    return NULL;
  }

  return out;
#else
  return path ? realpath(path, NULL) : NULL;
#endif
}
