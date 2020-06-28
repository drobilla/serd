// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_H
#define SERD_SRC_WORLD_H

#include "serd/error.h"
#include "serd/status.h"
#include "serd/world.h"

#include <stdarg.h>

struct SerdWorldImpl {
  SerdLimits  limits;
  SerdLogFunc error_func;
  void*       error_handle;
};

SerdStatus
serd_world_error(const SerdWorld* world, const SerdError* e);

SerdStatus
serd_world_verrorf(const SerdWorld* world,
                   SerdStatus       st,
                   const char*      fmt,
                   va_list          args);

#endif // SERD_SRC_WORLD_H
