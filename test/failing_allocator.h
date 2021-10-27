/*
  Copyright 2021 David Robillard <d@drobilla.net>

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

#ifndef SERD_FAILING_ALLOCATOR_H
#define SERD_FAILING_ALLOCATOR_H

#include "serd/serd.h"

#include <stddef.h>

/// An allocator that fails after some number of successes for testing
typedef struct {
  SerdAllocator base;          ///< Base allocator instance
  size_t        n_allocations; ///< Number of attempted allocations
  size_t        n_remaining;   ///< Number of remaining successful allocations
} SerdFailingAllocator;

SerdFailingAllocator
serd_failing_allocator(void);

#endif // SERD_FAILING_ALLOCATOR_H
