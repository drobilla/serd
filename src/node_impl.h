// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_IMPL_H
#define SERD_SRC_NODE_IMPL_H

#include "serd/node.h"

#include <stdint.h>

struct SerdNodeImpl {
  uint32_t      length;     ///< String length in bytes (without termination)
  SerdNodeFlags flags : 16; ///< Node flags
  SerdNodeType  type : 16;  ///< Node type
};

#endif // SERD_SRC_NODE_IMPL_H
