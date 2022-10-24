// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "compare.h"

#include "compare_data.h"
#include "statement.h"

#include <serd/field.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <zix/attributes.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

static int
object_view_compare(const SerdObjectView a, const SerdObjectView b)
{
  int cmp = 0;

  if ((cmp = ((int)a.type - (int)b.type)) ||
      (cmp = strcmp(a.string.data, b.string.data)) ||
      (cmp = ((int)a.flags - (int)b.flags)) ||
      !(a.flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE))) {
    return cmp;
  }

  assert(a.flags == b.flags);
  assert(a.flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE));
  assert(b.flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE));
  assert(a.meta.type == b.meta.type);

  return strcmp(a.meta.string.data, b.meta.string.data);
}

static int
node_id_compare(const SerdNodes* const nodes,
                const SerdNodeID       a,
                const SerdNodeID       b)
{
  assert(a);
  assert(b);

  const SerdObjectView av = serd_nodes_get_object(nodes, a);
  const SerdObjectView bv = serd_nodes_get_object(nodes, b);

  return object_view_compare(av, bv);
}

static int
node_id_wildcard_compare(const SerdNodes* const nodes,
                         const SerdNodeID       a,
                         const SerdNodeID       b)
{
  return (a && b) ? node_id_compare(nodes, a, b) : 0;
}

/// Compare an optional graph node with a node pattern
ZIX_PURE_FUNC static inline int
node_id_graph_compare(const SerdNodes* const nodes,
                      const SerdNodeID       a,
                      const SerdNodeID       b)
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

  return node_id_compare(nodes, a, b);
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
        ? node_id_graph_compare(
            cmp_data->nodes, s->nodes[SERD_GRAPH], t->nodes[SERD_GRAPH])
        : node_id_wildcard_compare(cmp_data->nodes, s->nodes[o], t->nodes[o]);

    if (c) {
      return c;
    }
  }

  return 0;
}
