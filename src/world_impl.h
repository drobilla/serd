// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_IMPL_H
#define SERD_SRC_WORLD_IMPL_H

#include "log.h"

#include "serd/world.h"
#include "zix/allocator.h"

#include <stdint.h>

struct SerdWorldImpl {
  SerdLimits    limits;
  ZixAllocator* allocator;
  SerdLog       log;
  uint32_t      next_document_id;
  uint32_t      next_blank_id;
  uint64_t      blank_buf[4U];
};

#endif // SERD_SRC_WORLD_IMPL_H
