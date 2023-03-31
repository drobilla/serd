// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "compare.h"

#include "statement_impl.h" // IWYU pragma: keep
#include "warnings.h"

#include "serd/field.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "zix/attributes.h"

#include <assert.h>

SERD_DISABLE_NULL_WARNINGS

/// Compare a mandatory node with a node pattern
ZIX_PURE_FUNC static inline int
serd_node_wildcard_compare(const SerdNode* ZIX_NONNULL  a,
                           const SerdNode* ZIX_NULLABLE b)
{
  assert(a);
  return b ? serd_node_compare(a, b) : 0;
}

/// Compare an optional graph node with a node pattern
static inline int
serd_node_graph_compare(const SerdNode* ZIX_NULLABLE a,
                        const SerdNode* ZIX_NULLABLE b)
{
  if (a == b) {
    return 0;
  }

  if (!a) {
    return -1;
  }

  if (!b) {
    return 1;
  }

  return serd_node_compare(a, b);
}

/// Compare statements lexicographically, ignoring graph
int
serd_triple_compare(const void* const x,
                    const void* const y,
                    const void* const user_data)
{
  const int* const           ordering = (const int*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 3U; ++i) {
    const int o = ordering[i];
    assert(o < 3);

    const int c = serd_node_compare(s->nodes[o], t->nodes[o]);
    if (c) {
      return c;
    }
  }

  return 0;
}

/**
   Compare statments with statement patterns lexicographically, ignoring graph.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
int
serd_triple_compare_pattern(const void* const x,
                            const void* const y,
                            const void* const user_data)
{
  const int* const           ordering = (const int*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 3U; ++i) {
    const int o = ordering[i];
    assert(o < 3);

    const int c = serd_node_wildcard_compare(s->nodes[o], t->nodes[o]);
    if (c) {
      return c;
    }
  }

  return 0;
}

/// Compare statements lexicographically
int
serd_quad_compare(const void* const x,
                  const void* const y,
                  const void* const user_data)
{
  const int* const           ordering = (const int*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 4U; ++i) {
    const int o = ordering[i];
    const int c =
      (o == SERD_GRAPH)
        ? serd_node_graph_compare(s->nodes[SERD_GRAPH], t->nodes[SERD_GRAPH])
        : serd_node_compare(s->nodes[o], t->nodes[o]);

    if (c) {
      return c;
    }
  }

  return 0;
}

/**
   Compare statments with statement patterns lexicographically.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
int
serd_quad_compare_pattern(const void* const x,
                          const void* const y,
                          const void* const user_data)
{
  const int* const           ordering = (const int*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 4U; ++i) {
    const int o = ordering[i];
    const int c =
      (o == SERD_GRAPH)
        ? serd_node_graph_compare(s->nodes[SERD_GRAPH], t->nodes[SERD_GRAPH])
        : serd_node_wildcard_compare(s->nodes[o], t->nodes[o]);

    if (c) {
      return c;
    }
  }

  return 0;
}

SERD_RESTORE_WARNINGS
