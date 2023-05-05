// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_MATCH_H
#define SERD_SRC_MATCH_H

#include "serd/node.h"
#include "zix/attributes.h"

#include <stdbool.h>

/// Return true if either node is null or both nodes are equal
static inline bool
serd_match_node(const SerdNode* const ZIX_NULLABLE a,
                const SerdNode* const ZIX_NULLABLE b)
{
  return !a || !b || serd_node_equals(a, b);
}

#endif // SERD_SRC_MATCH_H
