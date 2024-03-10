// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_H
#define SERD_SRC_WORLD_H

#include "serd/error.h"

struct SerdWorldImpl {
  SerdLogFunc error_func;
  void*       error_handle;
};

#endif // SERD_SRC_WORLD_H
