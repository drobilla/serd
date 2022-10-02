// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_H
#define SERD_SRC_WORLD_H

#include "serd/error.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"

#include <stdarg.h>
#include <stdint.h>

struct SerdWorldImpl {
  SerdLimits    limits;
  ZixAllocator* allocator;
  SerdLogFunc   error_func;
  void*         error_handle;
  uint32_t      next_document_id;
  uint32_t      next_blank_id;

  uint64_t blank_buf[6U];
};

SerdStatus
serd_world_error(const SerdWorld* world, const SerdError* e);

SerdStatus
serd_world_verrorf(const SerdWorld* world,
                   SerdStatus       st,
                   const char*      fmt,
                   va_list          args);

#endif // SERD_SRC_WORLD_H
