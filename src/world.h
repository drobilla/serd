// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_H
#define SERD_SRC_WORLD_H

#include "serd/error.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/world.h"

#include <stdint.h>
#include <stdio.h>

struct SerdWorldImpl {
  SerdErrorFunc error_func;
  void*         error_handle;
  uint32_t      next_blank_id;
  SerdNode*     blank_node;
};

/// Open a file configured for fast sequential reading
FILE*
serd_world_fopen(SerdWorld* world, const char* path, const char* mode);

SerdStatus
serd_world_error(const SerdWorld* world, const SerdError* e);

SerdStatus
serd_world_errorf(const SerdWorld* world, SerdStatus st, const char* fmt, ...);

#endif // SERD_SRC_WORLD_H
