// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_SYSTEM_H
#define SERD_SRC_SYSTEM_H

#include "zix/attributes.h"

#include <stdint.h>
#include <stdio.h>

#define SERD_PAGE_SIZE 4096

/// Allocate a buffer aligned to `alignment` bytes
ZIX_MALLOC_FUNC void*
serd_malloc_aligned(size_t alignment, size_t size);

/// Allocate an aligned buffer for I/O
ZIX_MALLOC_FUNC void*
serd_allocate_buffer(size_t size);

/// Free a buffer allocated with an aligned allocation function
void
serd_free_aligned(void* ptr);

/// Wrapper for getc that is compatible with SerdReadFunc
static inline size_t
serd_file_read_byte(void* buf, size_t size, size_t nmemb, void* stream)
{
  (void)size;
  (void)nmemb;

  const int c = getc((FILE*)stream);
  if (c == EOF) {
    *((uint8_t*)buf) = 0;
    return 0;
  }
  *((uint8_t*)buf) = (uint8_t)c;
  return 1;
}

#endif // SERD_SRC_SYSTEM_H
