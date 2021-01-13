// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_H
#define SERD_SRC_WORLD_H

#include "serd/log.h"
#include "serd/node.h"

#include <stdbool.h>
#include <stdint.h>

struct SerdWorldImpl {
  SerdLogFunc log_func;
  void*       log_handle;
  uint32_t    next_blank_id;
  SerdNode*   blank_node;

  bool stderr_color;
};

#endif // SERD_SRC_WORLD_H
