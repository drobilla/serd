// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_IMPL_H
#define SERD_SRC_WORLD_IMPL_H

#include <serd/error.h>
#include <zix/allocator.h>

struct SerdWorldImpl {
  ZixAllocator* allocator;
  SerdLogFunc   error_func;
  void*         error_handle;
};

#endif // SERD_SRC_WORLD_IMPL_H
