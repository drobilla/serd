// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_COMPARE_H
#define SERD_SRC_COMPARE_H

#include <zix/attributes.h>

/**
   Compare statements lexicographically, ignoring graph.

   Both arguments must point to complete statements (without wildcards).
*/
ZIX_PURE_FUNC int
serd_triple_compare(const void* x, const void* y, const void* user_data);

/**
   Compare statements with statement patterns lexicographically, ignoring graph.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
ZIX_PURE_FUNC int
serd_triple_compare_pattern(const void* x,
                            const void* y,
                            const void* user_data);

/**
   Compare statements lexicographically.

   Both arguments must point to complete statements (without wildcards).
*/
ZIX_PURE_FUNC int
serd_quad_compare(const void* x, const void* y, const void* user_data);

/**
   Compare statements with statement patterns lexicographically.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
ZIX_PURE_FUNC int
serd_quad_compare_pattern(const void* x, const void* y, const void* user_data);

#endif // ZIX_SRC_COMPARE_H
