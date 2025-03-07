// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"
#include "node_impl.h" // IWYU pragma: keep
#include "nodes_entry.h"
#include "nodes_key.h"
#include "string_utils.h"
#include "symbols.h"

// Define the types used in the data structure interfaces for type safety
#define ZIX_HASH_KEY_TYPE SerdNode
#define ZIX_HASH_RECORD_TYPE NodesEntry
#define ZIX_HASH_SEARCH_DATA_TYPE NodesKey
#define ZIX_RANK_TREE_ELEMENT_TYPE NodesEntry* ZIX_UNSPECIFIED

#include <serd/node_args.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/digest.h>
#include <zix/hash.h>
#include <zix/rank_tree.h>
#include <zix/status.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
   Allocator for allocating entries in the node hash table.

   This allocator implements only the methods used by the serd_node_*
   constructors, and transparently increases the allocation size so there is
   room for an extra NodesEntryHead at the start.  This allows the serd_node_*
   constructors to be reused here, even though the table stores entries (nodes
   with an extra header) rather than node pointers directly.
*/
typedef struct {
  ZixAllocator  base; ///< Implementation of ZixAllocator (base "class")
  ZixAllocator* real; ///< Underlying "real" memory allocator
} SerdNodesEntryAllocator;

ZIX_MALLOC_FUNC static void*
serd_nodes_entry_calloc(ZixAllocator* const allocator,
                        const size_t        nmemb,
                        const size_t        size)
{
  ZixAllocator* const real = ((SerdNodesEntryAllocator*)allocator)->real;

  (void)nmemb;
  assert(nmemb == 1); // We know we're being called for node.c allocation

  void* const ptr = real->calloc(real, 1U, sizeof(NodesEntryHead) + size);

  return ptr ? ((NodesEntryHead*)ptr + 1U) : NULL;
}

static void
serd_nodes_entry_free(ZixAllocator* const allocator, void* const ptr)
{
  ZixAllocator* const real = ((SerdNodesEntryAllocator*)allocator)->real;

  if (ptr) {
    real->free(real, (NodesEntryHead*)ptr - 1U);
  }
}

static SerdNodesEntryAllocator
serd_nodes_entry_allocator(ZixAllocator* const real)
{
  const SerdNodesEntryAllocator entry_allocator = {
    {
      NULL,
      serd_nodes_entry_calloc,
      NULL,
      serd_nodes_entry_free,
      NULL,
      NULL,
    },
    real ? real : zix_default_allocator(),
  };

  return entry_allocator;
}

struct SerdNodesImpl {
  SerdNodesEntryAllocator allocator; ///< Allocator for nodes and structures
  ZixHash*                hash;      ///< Hash table for looking up node IDs
  ZixRankTree*            tree;      ///< Rank tree for dereferencing node IDs
  size_t                  buf_size;  ///< Size of temporary buffer in bytes
  SerdNode*               buf;       ///< Temporary node buffer
};

static const SerdNodeFlags meta_mask = (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE);

static const SerdNode*
nodes_key(const NodesEntry* const entry)
{
  return &entry->node;
}

static NodesEntry*
node_entry(SerdNode* const node)
{
  return (NodesEntry*)((NodesEntryHead*)node - 1U);
}

static ZixHashCode
key_hash(const NodesKey key)
{
  ZixHashCode h = 0U;

  h = zix_digest(h, &key.type, sizeof(key.type));
  h = zix_digest(h, key.string.data, key.string.length);
  h = zix_digest(h, &key.meta, sizeof(key.meta));
  h = zix_digest(h, &key.flags, sizeof(key.flags));
  return h;
}

static ZixHashCode
nodes_hash(const SerdNode* const node)
{
  const ZixStringView string = serd_node_string_view(node);
  const SerdNodeFlags flags  = node->flags;
  const NodesKey      key    = {node->type, flags, string, node->meta};
  return key_hash(key);
}

static bool
node_equals_key(const SerdNode* const node, const NodesKey* const key)
{
  return node->type == key->type && node->flags == key->flags &&
         zix_string_view_equals(serd_node_string_view(node), key->string);
}

ZIX_PURE_FUNC static bool
nodes_equal(const SerdNode* const a, const SerdNode* const b)
{
  assert(a != b); // The hash table should never do this
  return (a->length == b->length && a->flags == b->flags &&
          a->type == b->type &&
          !memcmp(serd_node_string(a), serd_node_string(b), a->length) &&
          a->meta == b->meta);
}

static void
free_entry(SerdNodes* const nodes, NodesEntry* const entry)
{
  zix_free(&nodes->allocator.base, &entry->node);
}

SerdNodes*
serd_nodes_new(ZixAllocator* const allocator)
{
  SerdNodes* const nodes =
    (SerdNodes*)zix_calloc(allocator, 1, sizeof(SerdNodes));

  if (nodes) {
    nodes->allocator = serd_nodes_entry_allocator(allocator);

    // Start with enough space for any built-in type (xsd:decimal is 327 bytes)
    nodes->buf_size = 384U;
    if (!(nodes->buf = (SerdNode*)zix_calloc(allocator, nodes->buf_size, 1U))) {
      serd_nodes_free(nodes);
      return NULL;
    }

    if (!(nodes->hash =
            zix_hash_new(allocator, nodes_key, nodes_hash, nodes_equal))) {
      serd_nodes_free(nodes);
      return NULL;
    }

    if (!(nodes->tree = zix_rank_tree_new(allocator))) {
      serd_nodes_free(nodes);
      return NULL;
    }

    for (unsigned i = 1U; i < SERD_N_SYMBOLS; ++i) {
      const SerdNodeID id = serd_nodes_id(nodes, serd_a_uri(serd_symbols[i]));
      if (!id) {
        serd_nodes_free(nodes);
        return NULL;
      }

      assert(id == i);
    }
  }

  return nodes;
}

void
serd_nodes_free(SerdNodes* const nodes)
{
  if (nodes) {
    if (nodes->hash) {
      for (ZixHashIter i = zix_hash_begin(nodes->hash);
           i != zix_hash_end(nodes->hash);
           i = zix_hash_next(nodes->hash, i)) {
        free_entry(nodes, (NodesEntry*)zix_hash_get(nodes->hash, i));
      }
    }

    zix_rank_tree_free(nodes->tree, NULL, NULL);
    zix_hash_free(nodes->hash);
    zix_free(nodes->allocator.real, nodes->buf);
    zix_free(nodes->allocator.real, nodes);
  }
}

size_t
serd_nodes_size(const SerdNodes* const nodes)
{
  assert(nodes);

  return zix_hash_size(nodes->hash);
}

static const NodesEntry*
serd_nodes_manage_entry_at(SerdNodes* const        nodes,
                           NodesEntry* const       entry,
                           const ZixHashInsertPlan plan)
{
  assert(entry);
  assert(!entry->head.id);

  const size_t id = zix_rank_tree_size(nodes->tree) + 1U;
  if (id <= UINT32_MAX) {
    entry->head.id = (uint32_t)id;
    if (!zix_rank_tree_push(nodes->tree, entry)) {
      const ZixStatus st = zix_hash_insert_at(nodes->hash, plan, entry);
      assert(st != ZIX_STATUS_EXISTS);
      if (!st) {
        return entry;
      }
    }
  }

  free_entry(nodes, entry);
  return NULL;
}

static const NodesEntry*
serd_nodes_token(SerdNodes* const    nodes,
                 const SerdNodeType  type,
                 const ZixStringView string)
{
  if (type == SERD_NOTHING) {
    return NULL;
  }

  // Calculate a hash code for the token without actually constructing it
  const NodesKey    key  = {type, 0U, string, 0U};
  const ZixHashCode code = key_hash(key);

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  // Return an existing node if we have one
  const NodesEntry* const existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    assert(nodes_hash(&existing->node) == code);
    return existing;
  }

  // Otherwise, allocate and manage a new one
  SerdNode* const node =
    serd_node_new(&nodes->allocator.base, serd_a_token(type, string));
  return node ? serd_nodes_manage_entry_at(nodes, node_entry(node), plan)
              : NULL;
}

static const NodesEntry*
serd_nodes_existing_token(const SerdNodes* const nodes,
                          const SerdNodeType     type,
                          const ZixStringView    string)
{
  if (type == SERD_NOTHING) {
    return NULL;
  }

  // Calculate a hash code for the token without actually constructing it
  const NodesKey    key  = {type, 0U, string, 0U};
  const ZixHashCode code = key_hash(key);

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  return zix_hash_record_at(nodes->hash, plan);
}

static const NodesEntry*
serd_nodes_intern_internal(SerdNodes* const nodes, const SerdNode* const node)
{
  const ZixHashInsertPlan plan     = zix_hash_plan_insert(nodes->hash, node);
  NodesEntry* const       existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    return existing;
  }

  SerdNode* const new_node = serd_node_copy(&nodes->allocator.base, node);
  if (!new_node) {
    return NULL;
  }

  NodesEntry* const entry = node_entry(new_node);
  assert(!entry->head.id);

  return serd_nodes_manage_entry_at(nodes, entry, plan);
}

ZIX_PURE_FUNC static bool
is_langtag_string(const ZixStringView string)
{
  // First character must be a letter
  size_t i = 0;
  if (!string.length || !is_alpha(string.data[i])) {
    return false;
  }

  // First component must be all letters
  for (++i; i < string.length && string.data[i] != '-'; ++i) {
    if (!is_alpha(string.data[i])) {
      return false;
    }
  }

  // Following components can have letters and digits
  while (i < string.length) {
    for (++i; i < string.length && string.data[i] != '-'; ++i) {
      if (!is_alpha(string.data[i]) && !is_digit(string.data[i])) {
        return false;
      }
    }
  }

  return true;
}

ZIX_PURE_FUNC static bool
is_datatype_uri(const ZixStringView string)
{
  return serd_uri_string_has_scheme(string.data) &&
         !zix_string_view_equals(string, serd_symbols[RDF_LANGSTRING]);
}

static bool
is_valid_object(const SerdNodeFlags flags, const SerdTokenView meta)
{
  if ((flags & SERD_HAS_DATATYPE) && (flags & SERD_HAS_LANGUAGE)) {
    return false;
  }

  if (((flags & SERD_HAS_DATATYPE) && meta.type != SERD_URI) ||
      ((flags & SERD_HAS_LANGUAGE) && meta.type != SERD_LITERAL)) {
    return false;
  }

  if (((flags & SERD_HAS_DATATYPE) && !is_datatype_uri(meta.string)) ||
      ((flags & SERD_HAS_LANGUAGE) && !is_langtag_string(meta.string))) {
    return false;
  }

  return true;
}

static const NodesEntry*
serd_nodes_literal_internal(SerdNodes* const    nodes,
                            const ZixStringView string,
                            const SerdNodeFlags flags,
                            const SerdNodeID    meta_id)
{
  // Calculate a hash code for the literal without actually constructing it
  const NodesKey    key  = {SERD_LITERAL, flags, string, meta_id};
  const ZixHashCode code = key_hash(key);

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  // Return an existing node if we have one
  const NodesEntry* const existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    assert(nodes_hash(&existing->node) == code);
    return existing;
  }

  // Otherwise, allocate and manage a new one
  SerdNode* const node = serd_node_new(&nodes->allocator.base,
                                       serd_a_literal(string, flags, meta_id));
  return node ? serd_nodes_manage_entry_at(nodes, node_entry(node), plan)
              : NULL;
}

static const NodesEntry*
serd_nodes_existing_literal_internal(const SerdNodes* const nodes,
                                     const ZixStringView    string,
                                     const SerdNodeFlags    flags,
                                     const SerdNodeID       meta_id)
{
  // Calculate a hash code for the literal without actually constructing it
  const NodesKey    key  = {SERD_LITERAL, flags, string, meta_id};
  const ZixHashCode code = key_hash(key);

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  return zix_hash_record_at(nodes->hash, plan);
}

static const NodesEntry*
serd_nodes_existing_literal(const SerdNodes* const nodes,
                            const ZixStringView    string,
                            const SerdNodeFlags    flags,
                            const SerdNodeID       meta_id)
{
  const SerdTokenView meta = serd_nodes_get_token(nodes, meta_id);

  return is_valid_object(flags, meta)
           ? serd_nodes_existing_literal_internal(nodes, string, flags, meta_id)
           : NULL;
}

static const NodesEntry*
serd_nodes_existing_object(const SerdNodes* const nodes,
                           const ZixStringView    string,
                           const SerdNodeFlags    flags,
                           const SerdTokenView    meta)
{
  if (!is_valid_object(flags, meta)) {
    return NULL;
  }

  // Intern meta (datatype or language) node
  const NodesEntry* meta_entry = NULL;
  if ((flags & meta_mask) && !(meta_entry = serd_nodes_existing_token(
                                 nodes, meta.type, meta.string))) {
    return NULL;
  }

  const SerdNodeID meta_id = meta_entry ? meta_entry->head.id : 0U;
  return serd_nodes_existing_literal(nodes, string, flags, meta_id);
}

static const NodesEntry*
serd_nodes_object(SerdNodes* const    nodes,
                  const ZixStringView string,
                  const SerdNodeFlags flags,
                  const SerdTokenView meta)
{
  if (!is_valid_object(flags, meta)) {
    return NULL;
  }

  // Intern meta (datatype or language) node
  const NodesEntry* meta_entry = NULL;
  if ((flags & meta_mask) &&
      !(meta_entry = serd_nodes_token(nodes, meta.type, meta.string))) {
    return NULL;
  }

  // Calculate a hash code for the literal without actually constructing it
  const SerdNodeID meta_id = meta_entry ? meta_entry->head.id : 0U;
  return serd_nodes_literal_internal(nodes, string, flags, meta_id);
}

/*
  Some types here are cleverly hashed without allocating a node, but others
  simply allocate a new node and attempt to manage it.  It would be possible to
  calculate an in-place hash for some of them, but this is quite complicated
  and error-prone, so more difficult edge cases aren't yet implemented.
*/

static SerdNodeID
entry_id(const NodesEntry* const entry)
{
  return entry ? entry->head.id : 0U;
}

SerdNodeID
serd_nodes_id(SerdNodes* const nodes, const SerdNodeArgs args)
{
  assert(nodes);

  switch (args.type) {
  case SERD_NODE_ARGS_NODE_ID:
    return entry_id((args.data.as_node_id &&
                     args.data.as_node_id <= zix_rank_tree_size(nodes->tree))
                      ? zix_rank_tree_at(nodes->tree, args.data.as_node_id - 1U)
                      : NULL);

  case SERD_NODE_ARGS_URI:
  case SERD_NODE_ARGS_HOST_PATH:
  case SERD_NODE_ARGS_PREFIXED_NAME:
  case SERD_NODE_ARGS_JOINED_URI:
    break; // No fast path, use general approach

  case SERD_NODE_ARGS_TOKEN:
    return entry_id(serd_nodes_token(
      nodes, args.data.as_token.type, args.data.as_token.string));

  case SERD_NODE_ARGS_OBJECT:
    return entry_id(
      (args.data.as_object.type == SERD_LITERAL && args.data.as_object.flags)
        ? serd_nodes_object(nodes,
                            args.data.as_object.string,
                            args.data.as_object.flags,
                            args.data.as_object.meta)
        : serd_nodes_token(
            nodes, args.data.as_object.type, args.data.as_object.string));

  case SERD_NODE_ARGS_LITERAL:
    return entry_id(
      is_valid_object(args.data.as_literal.flags,
                      serd_nodes_get_token(nodes, args.data.as_literal.meta))
        ? serd_nodes_literal_internal(nodes,
                                      args.data.as_literal.string,
                                      args.data.as_literal.flags,
                                      args.data.as_literal.meta)
        : NULL);

  case SERD_NODE_ARGS_VALUE:
  case SERD_NODE_ARGS_DECIMAL:
  case SERD_NODE_ARGS_INTEGER:
    return serd_node_construct(nodes->buf_size, nodes->buf, args).status
             ? 0U
             : entry_id(serd_nodes_intern_internal(nodes, nodes->buf));

  case SERD_NODE_ARGS_HEX:
  case SERD_NODE_ARGS_BASE64:
    break; // No fast path, use general approach
  }

  // Construct the node into the temporary buffer, expanding it if necessary
  SerdStreamResult r = serd_node_construct(nodes->buf_size, nodes->buf, args);
  if (r.status == SERD_NO_SPACE) {
    void* const buf = zix_realloc(nodes->allocator.real, nodes->buf, r.count);
    if (!buf) {
      return 0U;
    }

    nodes->buf      = (SerdNode*)buf;
    nodes->buf_size = r.count;
    r               = serd_node_construct(nodes->buf_size, nodes->buf, args);
    assert(!r.status);
  } else if (r.status) {
    return 0U;
  }

  return entry_id(serd_nodes_intern_internal(nodes, nodes->buf));
}

SerdNodeID
serd_nodes_existing_id(const SerdNodes* const nodes, const SerdNodeArgs args)
{
  assert(nodes);

  switch (args.type) {
  case SERD_NODE_ARGS_NODE_ID:
    return (args.data.as_node_id <= zix_rank_tree_size(nodes->tree))
             ? args.data.as_node_id
             : 0U;

  case SERD_NODE_ARGS_URI:
  case SERD_NODE_ARGS_HOST_PATH:
  case SERD_NODE_ARGS_PREFIXED_NAME:
  case SERD_NODE_ARGS_JOINED_URI:
    break; // No fast path, use general approach

  case SERD_NODE_ARGS_TOKEN:
    return entry_id(serd_nodes_existing_token(
      nodes, args.data.as_token.type, args.data.as_token.string));

  case SERD_NODE_ARGS_OBJECT:
    return entry_id(
      (args.data.as_object.type == SERD_LITERAL && args.data.as_object.flags)
        ? serd_nodes_existing_object(nodes,
                                     args.data.as_object.string,
                                     args.data.as_object.flags,
                                     args.data.as_object.meta)
        : serd_nodes_existing_token(
            nodes, args.data.as_object.type, args.data.as_object.string));

  case SERD_NODE_ARGS_LITERAL:
    return entry_id(serd_nodes_existing_literal(nodes,
                                                args.data.as_literal.string,
                                                args.data.as_literal.flags,
                                                args.data.as_literal.meta));

  case SERD_NODE_ARGS_VALUE:
  case SERD_NODE_ARGS_DECIMAL:
  case SERD_NODE_ARGS_INTEGER:
    return serd_node_construct(nodes->buf_size, nodes->buf, args).status
             ? 0U
             : entry_id(zix_hash_record_at(
                 nodes->hash,
                 zix_hash_plan_insert(nodes->hash,
                                      (const SerdNode*)nodes->buf)));

  case SERD_NODE_ARGS_HEX:
  case SERD_NODE_ARGS_BASE64:
    break; // No fast path, use general approach
  }

  if (serd_node_construct(nodes->buf_size, nodes->buf, args).status) {
    /* If this is ZIX_STATUS_NO_MEM, then we know the node doesn't exist,
       because the buffer is large enough for any such node in the set. */
    return 0U;
  }

  return entry_id(zix_hash_record_at(
    nodes->hash,
    zix_hash_plan_insert(nodes->hash, (const SerdNode*)nodes->buf)));
}

SerdTokenView
serd_nodes_get_token(const SerdNodes* const nodes, const SerdNodeID id)
{
  assert(nodes);

  if (!id || id > zix_rank_tree_size(nodes->tree)) {
    return serd_no_token();
  }

  const NodesEntry* const entry = zix_rank_tree_at(nodes->tree, id - 1U);
  return serd_node_token_view(&entry->node);
}

SerdObjectView
serd_nodes_get_object(const SerdNodes* const nodes, const SerdNodeID id)
{
  assert(nodes);

  if (!id || id > zix_rank_tree_size(nodes->tree)) {
    return serd_no_object();
  }

  const NodesEntry* const entry = zix_rank_tree_at(nodes->tree, id - 1U);
  const SerdNode* const   node  = &entry->node;
  const SerdTokenView     meta  = serd_nodes_get_token(nodes, node->meta);
  return serd_object_view(
    node->type, serd_node_string_view(node), node->flags, meta);
}
