// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_SYSTEM_H
#define SERD_SRC_SYSTEM_H

#include "serd/attributes.h"

#include <stdio.h>

#define SERD_PAGE_SIZE 4096

/// Write the message for a system error code (like errno) to a buffer
int
serd_system_strerror(int errnum, char* buf, size_t buflen);

/// Allocate a buffer aligned to `alignment` bytes
SERD_MALLOC_FUNC
void*
serd_malloc_aligned(size_t alignment, size_t size);

/// Allocate a zeroed buffer aligned to `alignment` bytes
SERD_MALLOC_FUNC
void*
serd_calloc_aligned(size_t alignment, size_t size);

/// Allocate an aligned buffer for I/O
SERD_MALLOC_FUNC
void*
serd_allocate_buffer(size_t size);

/// Free a buffer allocated with an aligned allocation function
void
serd_free_aligned(void* ptr);

#endif // SERD_SRC_SYSTEM_H
