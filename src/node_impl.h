// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_IMPL_H
#define SERD_SRC_NODE_IMPL_H

#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>

#include <stdint.h>

struct SerdNodeImpl {
  SerdNodeType  type : 16;  ///< Node type
  SerdNodeFlags flags : 16; ///< Node flags
  SerdNodeID    meta;       ///< Language tag or datatype URI ID
  uint32_t      length;     ///< Length in bytes (without terminator)
};

#endif // SERD_SRC_NODE_IMPL_H
