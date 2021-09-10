// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "system.h"

#include "memory.h"
#include "serd_config.h"

#include "serd/memory.h"
#include "serd/string.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN 1
#  include <malloc.h>
#  include <windows.h>
#else
#  include <limits.h>
#  ifndef PATH_MAX
#    include <unistd.h>
#  endif
#endif

#include <assert.h>
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

char*
serd_canonical_path(SerdAllocator* const allocator, const char* const path)
{
  assert(path);

#if defined(_WIN32)
  // Microsoft got this one right: measure, allocate, resolve
  const DWORD size = GetFullPathName(path, 0, NULL, NULL);
  if (size == 0) {
    return NULL;
  }

  char* const out = (char*)serd_acalloc(allocator, size, 1);
  if (out) {
    const DWORD ret = GetFullPathName(path, MAX_PATH, out, NULL);
    if (ret == 0 || ret >= size) {
      serd_afree(allocator, out);
      return NULL;
    }
  }

  return out;

#elif defined(PATH_MAX)
  // Some POSIX systems have a static PATH_MAX so we can resolve on the stack
  char  result[PATH_MAX] = {0};
  char* resolved_path    = realpath(path, result);
  if (!resolved_path) {
    return NULL;
  }

  const size_t len = strlen(resolved_path);
  char* const  out = (char*)serd_acalloc(allocator, len + 1, 1);
  if (out) {
    memcpy(out, resolved_path, len + 1);
  }
  return out;

#else
  // Others don't so we have to query PATH_MAX at runtime to allocate the result
  long path_max = pathconf(path, _PC_PATH_MAX);
  if (path_max <= 0) {
    path_max = SERD_PAGE_SIZE;
  }

  char* const out = (char*)serd_acalloc(allocator, path_max, 1);
  return out ? realpath(path, out) : NULL;
#endif
}
