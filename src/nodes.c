/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#include "node.h"

#include "serd/serd.h"
#include "zix/common.h"
#include "zix/digest.h"
#include "zix/hash.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t    refs;
  SerdNode* node;
} NodesEntry;

typedef struct {
  size_t          refs;
  const SerdNode* node;
} NodesSearchKey;

struct SerdNodesImpl {
  ZixHash* hash;
};

static uint32_t
nodes_hash(const void* n)
{
  const SerdNode* node = ((const NodesEntry*)n)->node;

  // FIXME
  /* return zix_digest_add_64( */
  /*   zix_digest_start(), node, serd_node_total_size(node)); */
  return zix_digest_add(zix_digest_start(), node, serd_node_total_size(node));
}

static bool
nodes_equal(const void* a, const void* b)
{
  const SerdNode* a_node = ((const NodesEntry*)a)->node;
  const SerdNode* b_node = ((const NodesEntry*)b)->node;
  const size_t    a_size = serd_node_total_size(a_node);
  const size_t    b_size = serd_node_total_size(b_node);
  return ((a_node == b_node) ||
          (a_size == b_size && !memcmp(a_node, b_node, a_size)));
}

static void
free_entry(void* value, void* user_data)
{
  (void)user_data;

  NodesEntry* entry = (NodesEntry*)value;
  serd_node_free(entry->node);
}

SerdNodes*
serd_nodes_new(void)
{
  SerdNodes* nodes = (SerdNodes*)calloc(1, sizeof(SerdNodes));
  nodes->hash      = zix_hash_new(nodes_hash, nodes_equal, sizeof(NodesEntry));
  return nodes;
}

void
serd_nodes_free(SerdNodes* nodes)
{
  if (nodes) {
    zix_hash_foreach(nodes->hash, free_entry, nodes);
    zix_hash_free(nodes->hash);
    free(nodes);
  }
}

size_t
serd_nodes_size(const SerdNodes* nodes)
{
  return zix_hash_size(nodes->hash);
}

const SerdNode*
serd_nodes_intern(SerdNodes* nodes, const SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  NodesSearchKey key      = {1, node};
  NodesEntry*    inserted = NULL;

  const ZixStatus st = zix_hash_insert(nodes->hash, &key, (void**)&inserted);
  if (st == ZIX_STATUS_SUCCESS) {
    inserted->node = serd_node_copy(node);
  } else if (st == ZIX_STATUS_EXISTS) {
    assert(serd_node_equals(inserted->node, node));
    ++inserted->refs;
  }

  return inserted ? inserted->node : NULL;
}

const SerdNode*
serd_nodes_manage(SerdNodes* nodes, SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  NodesSearchKey key      = {1, node};
  NodesEntry*    inserted = NULL;

  const ZixStatus st = zix_hash_insert(nodes->hash, &key, (void**)&inserted);
  if (st == ZIX_STATUS_EXISTS) {
    assert(serd_node_equals(inserted->node, node));
    serd_node_free(node);
    ++inserted->refs;
  }

  return inserted ? inserted->node : NULL;
}

void
serd_nodes_deref(SerdNodes* nodes, const SerdNode* node)
{
  NodesSearchKey key   = {1, node};
  NodesEntry*    entry = (NodesEntry*)zix_hash_find(nodes->hash, &key);
  if (entry && --entry->refs == 0) {
    SerdNode* const intern_node = entry->node;
    zix_hash_remove(nodes->hash, entry);
    serd_node_free(intern_node);
  }
}
