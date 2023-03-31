// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_COMPARE_H
#define SERD_SRC_COMPARE_H

#include "zix/attributes.h"

/// Compare statements lexicographically, ignoring graph
ZIX_PURE_FUNC int
serd_triple_compare(const void* x, const void* y, const void* user_data);

/**
   Compare statments with statement patterns lexicographically, ignoring graph.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
ZIX_PURE_FUNC int
serd_triple_compare_pattern(const void* x,
                            const void* y,
                            const void* user_data);

/// Compare statements lexicographically
ZIX_PURE_FUNC int
serd_quad_compare(const void* x, const void* y, const void* user_data);

/**
   Compare statments with statement patterns lexicographically.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
ZIX_PURE_FUNC int
serd_quad_compare_pattern(const void* x, const void* y, const void* user_data);

#endif // ZIX_SRC_COMPARE_H
