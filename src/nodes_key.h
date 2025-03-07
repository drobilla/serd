// Copyright 2021-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODES_KEY_H
#define SERD_SRC_NODES_KEY_H

#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <zix/string_view.h>

typedef struct {
  SerdNodeType  type : 16;  ///< Node type
  SerdNodeFlags flags : 16; ///< Node flags
  SerdNodeID    meta;       ///< ID of language string or datatype URI
  ZixStringView string;     ///< Node string
} NodesKey;

#endif // SERD_SRC_NODES_KEY_H
