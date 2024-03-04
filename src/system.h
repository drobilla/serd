// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_SYSTEM_H
#define SERD_SRC_SYSTEM_H

#include <stdint.h>
#include <stdio.h>

#define SERD_PAGE_SIZE 4096

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
