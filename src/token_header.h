// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_TOKEN_HEADER_H
#define SERD_SRC_TOKEN_HEADER_H

#include <serd/node_flags.h>
#include <serd/node_type.h>

#include <stdint.h>

typedef struct {
  SerdNodeType  type : 16;  ///< Node type
  SerdNodeFlags flags : 16; ///< Node flags
  uint32_t      length;     ///< Length in bytes (not including null)
} TokenHeader;

#endif // SERD_SRC_TOKEN_HEADER_H
