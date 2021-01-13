// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_H
#define SERD_SRC_WORLD_H

#include "log.h"

#include "serd/node.h"
#include "serd/world.h"
#include "zix/allocator.h"

#include <stdint.h>

struct SerdWorldImpl {
  SerdLimits    limits;
  ZixAllocator* allocator;
  SerdLog       log;
  uint32_t      next_blank_id;
  uint32_t      next_document_id;
  SerdNode*     blank_node;
};

#endif // SERD_SRC_WORLD_H
