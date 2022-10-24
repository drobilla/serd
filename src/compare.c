// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "compare.h"

#include "compare_data.h"
#include "nodes_internal.h"
#include "statement.h"

#include <serd/field.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <zix/attributes.h>

#include <assert.h>
#include <stdint.h>

// Generally the LHS (a) is from the tree and the RHS (b) from the pattern

ZIX_PURE_FUNC static int
node_id_compare(const SerdNodes* const nodes,
                const SerdNodeID       a,
                const SerdNodeID       b)
{
  assert(a);
  assert(b);
  return serd_nodes_compare(nodes, a, b);
}

ZIX_PURE_FUNC static int
node_id_wildcard_compare(const SerdNodes* const nodes,
                         const SerdNodeID       a,
                         const SerdNodeID       b)
{
  assert(a);
  return b ? node_id_compare(nodes, a, b) : 0;
}

ZIX_PURE_FUNC static int
node_id_graph_compare(const SerdNodes* const nodes,
                      const SerdNodeID       a,
                      const SerdNodeID       b)
{
  return (a == b) ? 0 : !a ? -1 : !b ? 1 : node_id_compare(nodes, a, b);
}

ZIX_PURE_FUNC static int
node_id_graph_wildcard_compare(const SerdNodes* const nodes,
                               const SerdNodeID       a,
                               const SerdNodeID       b)
{
  return !b ? 0 : node_id_graph_compare(nodes, a, b);
}

int
serd_triple_compare(const void* const x,
                    const void* const y,
                    const void* const user_data)
{
  const CompareData* const   cmp_data = (const CompareData*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 3U; ++i) {
    const uint8_t o = cmp_data->ordering[i];
    assert(o < 3U);

    const int c = node_id_compare(cmp_data->nodes, s->nodes[o], t->nodes[o]);
    if (c) {
      return c;
    }
  }

  return 0;
}

int
serd_triple_compare_pattern(const void* const x,
                            const void* const y,
                            const void* const user_data)
{
  const CompareData* const   cmp_data = (const CompareData*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 3U; ++i) {
    const uint8_t o = cmp_data->ordering[i];
    assert(o < 3U);

    const int c =
      node_id_wildcard_compare(cmp_data->nodes, s->nodes[o], t->nodes[o]);
    if (c) {
      return c;
    }
  }

  return 0;
}

int
serd_quad_compare(const void* const x,
                  const void* const y,
                  const void* const user_data)
{
  const CompareData* const   cmp_data = (const CompareData*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 4U; ++i) {
    const uint8_t o = cmp_data->ordering[i];

    const int c =
      (o == SERD_GRAPH)
        ? node_id_graph_compare(
            cmp_data->nodes, s->nodes[SERD_GRAPH], t->nodes[SERD_GRAPH])
        : node_id_compare(cmp_data->nodes, s->nodes[o], t->nodes[o]);

    if (c) {
      return c;
    }
  }

  return 0;
}

int
serd_quad_compare_pattern(const void* const x,
                          const void* const y,
                          const void* const user_data)
{
  const CompareData* const   cmp_data = (const CompareData*)user_data;
  const SerdStatement* const s        = (const SerdStatement*)x;
  const SerdStatement* const t        = (const SerdStatement*)y;

  for (unsigned i = 0U; i < 4U; ++i) {
    const uint8_t o = cmp_data->ordering[i];

    const int c =
      (o == SERD_GRAPH)
        ? node_id_graph_wildcard_compare(
            cmp_data->nodes, s->nodes[SERD_GRAPH], t->nodes[SERD_GRAPH])
        : node_id_wildcard_compare(cmp_data->nodes, s->nodes[o], t->nodes[o]);

    if (c) {
      return c;
    }
  }

  return 0;
}
