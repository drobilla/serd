// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "memory.h"

#include "serd/memory.h"
#include "zix/allocator.h"

SerdAllocator*
serd_default_allocator(void)
{
  /* Note that SerdAllocator is intentionally the same as ZixAllocator.  It
     only exists to avoid exposing the zix API in the public serd API, which
     I'm not sure would be appropriate. */

  return (SerdAllocator*)zix_default_allocator();
}
