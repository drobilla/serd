/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "compare.h"

#include "node.h"
#include "statement.h"

#include "serd/serd.h"

#include <assert.h>

/// Compare a mandatory node with a node pattern
static inline int
serd_node_wildcard_compare(const SerdNode* SERD_NONNULL  a,
                           const SerdNode* SERD_NULLABLE b)
{
  assert(a);
  return b ? serd_node_compare(a, b) : 0;
}

/// Compare an optional graph node with a node pattern
static inline int
serd_node_graph_compare(const SerdNode* SERD_NULLABLE a,
                        const SerdNode* SERD_NULLABLE b)
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

  /* fprintf(stderr, */
  /*         "TCMP \t%s %s %s\n    \t%s %s %s\n", */
  /*         serd_node_string(serd_statement_subject(s)), */
  /*         serd_node_string(serd_statement_predicate(s)), */
  /*         serd_node_string(serd_statement_object(s)), */
  /*         serd_node_string(serd_statement_subject(t)), */
  /*         serd_node_string(serd_statement_predicate(t)), */
  /*         serd_node_string(serd_statement_object(t))); */

  for (unsigned i = 0u; i < 3u; ++i) {
    const int o = ordering[i];
    assert(o < 3);

    const int c = serd_node_compare(s->nodes[o], t->nodes[o]);
    if (c) {
      return c;
    }
  }

  /* fprintf(stderr, "\t=> 0\n"); */
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

  /* fprintf( */
  /*   stderr, */
  /*   "TCMP* \t%s %s %s\n     \t%s %s %s\n", */
  /*   serd_statement_subject(s) ? serd_node_string(serd_statement_subject(s))
   */
  /*                             : "*", */
  /*   serd_statement_predicate(s) ?
   * serd_node_string(serd_statement_predicate(s)) */
  /*                               : "*", */
  /*   serd_statement_object(s) ? serd_node_string(serd_statement_object(s)) :
   * "*", */
  /*   serd_statement_subject(t) ? serd_node_string(serd_statement_subject(t))
   */
  /*                             : "*", */
  /*   serd_statement_predicate(t) ?
   * serd_node_string(serd_statement_predicate(t)) */
  /*                               : "*", */
  /*   serd_statement_object(t) ? serd_node_string(serd_statement_object(t)) */
  /*                            : "*"); */

  for (unsigned i = 0u; i < 3u; ++i) {
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

  for (unsigned i = 0u; i < 4u; ++i) {
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

  for (unsigned i = 0u; i < 4u; ++i) {
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
