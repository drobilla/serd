// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_IMPL_H
#define SERD_SRC_NODE_IMPL_H

#include "serd/node.h"

#include <stddef.h>

struct SerdNodeImpl {
  size_t        length; ///< Length in bytes (not including null)
  SerdNodeFlags flags;  ///< Node flags
  SerdNodeType  type;   ///< Node type
};

#endif // SERD_SRC_NODE_IMPL_H
