// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_IMPL_H
#define SERD_SRC_WORLD_IMPL_H

#include <serd/error.h>
#include <serd/world.h>
#include <zix/allocator.h>

struct SerdWorldImpl {
  SerdLimits    limits;
  ZixAllocator* allocator;
  SerdLogFunc   error_func;
  void*         error_handle;
};

#endif // SERD_SRC_WORLD_IMPL_H
