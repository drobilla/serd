// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"
#include "node_impl.h"
#include "nodes_entry.h"
#include "nodes_internal.h"
#include "nodes_key.h"
#include "string_utils.h"
#include "symbols.h"

// Define the types used in the data structure interfaces for type safety
#define ZIX_HASH_KEY_TYPE SerdNode
#define ZIX_HASH_RECORD_TYPE NodesEntry
#define ZIX_HASH_SEARCH_DATA_TYPE NodesKey
#define ZIX_HASH_EXT_DATA_TYPE const SerdNodes* ZIX_NONNULL
#define ZIX_RANK_TREE_ELEMENT_TYPE NodesEntry* ZIX_UNSPECIFIED

#include <serd/literal_view.h>
#include <serd/node_args.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <serd/struct_literal.h>
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

#define SERD_MAX_STRING_LENGTH (UINT32_MAX - 3U)
#define SERD_MAX_NODE_SIZE (SERD_MAX_STRING_LENGTH - sizeof(SerdNode))

struct SerdNodesImpl {
  ZixAllocator* allocator; ///< Allocator for nodes and structures
  ZixHash*      hash;      ///< Hash table for looking up node IDs
  ZixRankTree*  tree;      ///< Rank tree for dereferencing node IDs
  size_t        buf_size;  ///< Size of temporary buffer in bytes
  SerdNode*     buf;       ///< Temporary node buffer
};

static const SerdNodeFlags meta_mask = (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE);

static const SerdNode*
nodes_key(const NodesEntry* const entry)
{
  return &entry->node;
}

static ZixHashCode
token_hash(const ZixHashCode seed, const SerdTokenView token)
{
  return zix_digest(zix_digest(seed, &token.type, sizeof(token.type)),
                    token.string.data,
                    token.string.length);
}

static ZixHashCode
key_hash(const NodesKey* key, const SerdNodes* const nodes)
{
  ZixHashCode h = token_hash(0U, serd_token_view(key->type, key->string));

  if (key->meta) {
    h = token_hash(h, serd_nodes_get_token(nodes, key->meta));
  }

  return h;
}

static ZixHashCode
nodes_hash(const SerdNode* const node, const SerdNodes* const nodes)
{
  ZixHashCode h = token_hash(0U, serd_node_token_view(node));

  if (node->meta) {
    h = token_hash(h, serd_nodes_get_token(nodes, node->meta));
  }

  return h;
}

ZIX_PURE_FUNC static bool
nodes_equal(const SerdNode* const a, const SerdNode* const b)
{
  return (a->type == b->type && a->flags == b->flags && a->meta == b->meta &&
          a->length == b->length &&
          !memcmp(serd_node_string(a), serd_node_string(b), a->length));
}

static bool
node_equals_key(const SerdNode* const node, const NodesKey* const key)
{
  return node->type == key->type && node->flags == key->flags &&
         zix_string_view_equals(serd_node_string_view(node), key->string) &&
         node->meta == key->meta;
}

static void
free_entry(SerdNodes* const nodes, NodesEntry* const entry)
{
  zix_free(nodes->allocator, entry);
}

SerdNodes*
serd_nodes_new(ZixAllocator* const allocator)
{
  SerdNodes* const nodes =
    (SerdNodes*)zix_calloc(allocator, 1, sizeof(SerdNodes));

  if (nodes) {
    nodes->allocator = allocator;

    // Start with enough space for any built-in type (xsd:decimal is 327 bytes)
    nodes->buf_size = 384U;
    if (!(nodes->buf = (SerdNode*)zix_calloc(allocator, nodes->buf_size, 1U))) {
      serd_nodes_free(nodes);
      return NULL;
    }

    if (!(nodes->hash = zix_hash_new_ext(
            allocator, nodes_key, nodes_hash, nodes, nodes_equal))) {
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
    zix_free(nodes->allocator, nodes->buf);
    zix_free(nodes->allocator, nodes);
  }
}

static const NodesEntry*
manage_entry_at(SerdNodes* const        nodes,
                NodesEntry* const       entry,
                const ZixHashInsertPlan plan)
{
  if (!entry) {
    return NULL;
  }

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

static NodesEntry*
new_entry(SerdNodes* const nodes, const SerdNodeArgs args)
{
  SerdStreamResult r = serd_node_construct(args, 0, NULL);
  assert(r.status == SERD_NO_SPACE); // Only used for token/literal

  NodesEntry* const entry = (NodesEntry*)zix_calloc(
    nodes->allocator, 1U, sizeof(NodesEntryHead) + r.count);

  if (entry) {
    (void)serd_node_construct(args, r.count, &entry->node);
  }

  return entry;
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
  const NodesKey    key  = {type, 0U, 0U, string};
  const ZixHashCode code = token_hash(0U, serd_token_view(type, string));

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  // Return an existing node if we have one
  const NodesEntry* const existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    assert(nodes_hash(&existing->node, nodes) == code);
    return existing;
  }

  // Otherwise, allocate and manage a new one
  return manage_entry_at(
    nodes, new_entry(nodes, serd_a_token(type, string)), plan);
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
  const NodesKey    key  = {type, 0U, 0U, string};
  const ZixHashCode code = token_hash(0U, serd_token_view(type, string));

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  return zix_hash_record_at(nodes->hash, plan);
}

// FIXME: ?
ZIX_CONST_FUNC static size_t
pad_length(const size_t length)
{
  const size_t terminated = length + 1U;
  const size_t padded     = (terminated + 3U) & ~0x03U;
  assert(padded % 4U == 0U);
  return padded;
}

static const NodesEntry*
serd_nodes_intern_internal(SerdNodes* const nodes, const SerdNode* const node)
{
  const ZixHashInsertPlan plan     = zix_hash_plan_insert(nodes->hash, node);
  NodesEntry* const       existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    return existing;
  }

  const size_t      size  = sizeof(NodesEntry) + pad_length(node->length);
  NodesEntry* const entry = (NodesEntry*)zix_calloc(nodes->allocator, 1U, size);

  if (entry) {
    memcpy(&entry->node, node, sizeof(SerdNode) + node->length);
  }

  return manage_entry_at(nodes, entry, plan);
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
is_valid_meta_type(const SerdNodeFlags flags, const SerdNodeType meta_type)
{
  if ((flags & SERD_HAS_DATATYPE) && (flags & SERD_HAS_LANGUAGE)) {
    return false;
  }

  if (((flags & SERD_HAS_DATATYPE) && meta_type != SERD_URI) ||
      ((flags & SERD_HAS_LANGUAGE) && meta_type != SERD_LITERAL)) {
    return false;
  }

  return true;
}

static bool
is_valid_meta(const SerdNodeFlags flags, const SerdTokenView meta)
{
  return is_valid_meta_type(flags, meta.type) &&
         (!(flags & SERD_HAS_DATATYPE) || is_datatype_uri(meta.string)) &&
         (!(flags & SERD_HAS_LANGUAGE) || is_langtag_string(meta.string));
}

static const NodesEntry*
serd_nodes_literal_internal(SerdNodes* const    nodes,
                            const ZixStringView string,
                            const SerdNodeFlags flags,
                            const SerdNodeID    meta_id)
{
  // Calculate a hash code for the literal without actually constructing it
  const NodesKey    key  = {SERD_LITERAL, (uint16_t)flags, meta_id, string};
  const ZixHashCode code = key_hash(&key, nodes);

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  // Return an existing node if we have one
  const NodesEntry* const existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    assert(nodes_hash(&existing->node, nodes) == code);
    return existing;
  }

  // Otherwise, allocate and manage a new one
  return manage_entry_at(
    nodes, new_entry(nodes, serd_a_literal(string, flags, meta_id)), plan);
}

static const NodesEntry*
serd_nodes_existing_literal_internal(const SerdNodes* const nodes,
                                     const ZixStringView    string,
                                     const SerdNodeFlags    flags,
                                     const SerdNodeID       meta_id)
{
  // Calculate a hash code for the literal without actually constructing it
  const NodesKey    key  = {SERD_LITERAL, (uint16_t)flags, meta_id, string};
  const ZixHashCode code = key_hash(&key, nodes);

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
  return is_valid_meta_type(flags, serd_nodes_type(nodes, meta_id))
           ? serd_nodes_existing_literal_internal(nodes, string, flags, meta_id)
           : NULL;
}

static const NodesEntry*
serd_nodes_existing_object(const SerdNodes* const nodes,
                           const ZixStringView    string,
                           const SerdNodeFlags    flags,
                           const SerdTokenView    meta)
{
  if (!is_valid_meta(flags, meta)) {
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
  if (!is_valid_meta(flags, meta)) {
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
    return entry_id((args.data.node_id &&
                     args.data.node_id <= zix_rank_tree_size(nodes->tree))
                      ? zix_rank_tree_at(nodes->tree, args.data.node_id - 1U)
                      : NULL);

  case SERD_NODE_ARGS_URI:
  case SERD_NODE_ARGS_HOST_PATH:
  case SERD_NODE_ARGS_PREFIXED_NAME:
  case SERD_NODE_ARGS_JOINED_URI:
    break; // No fast path, use general approach

  case SERD_NODE_ARGS_TOKEN:
    return entry_id(
      serd_nodes_token(nodes, args.data.token.type, args.data.token.string));

  case SERD_NODE_ARGS_OBJECT:
    return entry_id(
      (args.data.object.type == SERD_LITERAL && args.data.object.flags)
        ? serd_nodes_object(nodes,
                            args.data.object.string,
                            args.data.object.flags,
                            args.data.object.meta)
        : serd_nodes_token(
            nodes, args.data.object.type, args.data.object.string));

  case SERD_NODE_ARGS_LITERAL:
    return entry_id(
      is_valid_meta(args.data.literal.flags,
                    serd_nodes_get_token(nodes, args.data.literal.meta))
        ? serd_nodes_literal_internal(nodes,
                                      args.data.literal.string,
                                      args.data.literal.flags,
                                      args.data.literal.meta)
        : NULL);

  case SERD_NODE_ARGS_VALUE:
  case SERD_NODE_ARGS_DECIMAL:
  case SERD_NODE_ARGS_INTEGER:
    return serd_node_construct(args, nodes->buf_size, nodes->buf).status
             ? 0U
             : entry_id(serd_nodes_intern_internal(nodes, nodes->buf));

  case SERD_NODE_ARGS_HEX:
  case SERD_NODE_ARGS_BASE64:
    break; // No fast path, use general approach
  }

  // Construct the node into the temporary buffer, expanding it if necessary
  SerdStreamResult r = serd_node_construct(args, nodes->buf_size, nodes->buf);
  if (r.status == SERD_NO_SPACE && r.count <= SERD_MAX_NODE_SIZE) {
    void* const buf = zix_realloc(nodes->allocator, nodes->buf, r.count);
    if (!buf) {
      return 0U;
    }

    nodes->buf      = (SerdNode*)buf;
    nodes->buf_size = r.count;
    r               = serd_node_construct(args, nodes->buf_size, nodes->buf);
    assert(!r.status);
  } else if (r.status) {
    return 0U;
  }

  return entry_id(serd_nodes_intern_internal(nodes, nodes->buf));
}

SerdNodeID
serd_nodes_find(const SerdNodes* const nodes, const SerdNodeArgs args)
{
  assert(nodes);

  switch (args.type) {
  case SERD_NODE_ARGS_NODE_ID:
    return (args.data.node_id <= zix_rank_tree_size(nodes->tree))
             ? args.data.node_id
             : 0U;

  case SERD_NODE_ARGS_URI:
  case SERD_NODE_ARGS_HOST_PATH:
  case SERD_NODE_ARGS_PREFIXED_NAME:
  case SERD_NODE_ARGS_JOINED_URI:
    break; // No fast path, use general approach

  case SERD_NODE_ARGS_TOKEN:
    return entry_id(serd_nodes_existing_token(
      nodes, args.data.token.type, args.data.token.string));

  case SERD_NODE_ARGS_OBJECT:
    return entry_id(
      (args.data.object.type == SERD_LITERAL && args.data.object.flags)
        ? serd_nodes_existing_object(nodes,
                                     args.data.object.string,
                                     args.data.object.flags,
                                     args.data.object.meta)
        : serd_nodes_existing_token(
            nodes, args.data.object.type, args.data.object.string));

  case SERD_NODE_ARGS_LITERAL:
    return entry_id(serd_nodes_existing_literal(nodes,
                                                args.data.literal.string,
                                                args.data.literal.flags,
                                                args.data.literal.meta));

  case SERD_NODE_ARGS_VALUE:
  case SERD_NODE_ARGS_DECIMAL:
  case SERD_NODE_ARGS_INTEGER:
    return serd_node_construct(args, nodes->buf_size, nodes->buf).status
             ? 0U
             : entry_id(zix_hash_record_at(
                 nodes->hash,
                 zix_hash_plan_insert(nodes->hash,
                                      (const SerdNode*)nodes->buf)));

  case SERD_NODE_ARGS_HEX:
  case SERD_NODE_ARGS_BASE64:
    break; // No fast path, use general approach
  }

  if (serd_node_construct(args, nodes->buf_size, nodes->buf).status) {
    /* If this is ZIX_STATUS_NO_MEM, then we know the node doesn't exist,
       because the buffer is large enough for any such node in the set. */
    return 0U;
  }

  return entry_id(zix_hash_record_at(
    nodes->hash,
    zix_hash_plan_insert(nodes->hash, (const SerdNode*)nodes->buf)));
}

static NodesEntry*
copy_entry(ZixAllocator* const allocator,
           const NodesEntry*   entry,
           const SerdNodeID    meta)
{
  assert(entry);

  const size_t      size = sizeof(NodesEntry) + pad_length(entry->node.length);
  NodesEntry* const copy = (NodesEntry*)zix_calloc(allocator, 1U, size);
  if (copy) {
    memcpy(copy, entry, size);
    copy->head.id   = 0U;
    copy->node.meta = meta;
  }

  return copy;
}

SerdNodeID
serd_nodes_crib(SerdNodes* const       nodes,
                const SerdNodes* const source,
                const SerdNodeID       id)
{
  if (nodes == source) {
    return id;
  }

  if (!id || id > zix_rank_tree_size(source->tree)) {
    return 0U;
  }

  // Use the source node as a key, except with a cribbed meta ID
  const uint32_t          old_index = id - 1U;
  const NodesEntry* const old_entry = zix_rank_tree_at(source->tree, old_index);
  const SerdNode* const   old_node  = &old_entry->node;
  const NodesKey          key       = {
    old_node->type,
    old_node->flags,
    serd_nodes_crib(nodes, source, old_node->meta),
    serd_node_string_view(old_node),
  };

  // Search for an equivalent node in the target
  const ZixHashCode       code = key_hash(&key, nodes);
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_key, &key);

  // If an equivalent node exists in the target, return its ID
  const NodesEntry* const existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    return existing->head.id;
  }

  // Otherwise, manage a copy of the source entry (but with cribbed meta)
  const NodesEntry* const entry = manage_entry_at(
    nodes, copy_entry(nodes->allocator, old_entry, key.meta), plan);

  return entry ? (SerdNodeID)zix_rank_tree_size(nodes->tree) : 0U;
}

static const NodesEntry*
find_entry(const SerdNodes* const nodes, const SerdNodeID id)
{
  return (!id || id > zix_rank_tree_size(nodes->tree))
           ? NULL
           : (const NodesEntry*)zix_rank_tree_at(nodes->tree, id - 1U);
}

SerdNodeType
serd_nodes_type(const SerdNodes* const nodes, const SerdNodeID id)
{
  assert(nodes);

  const NodesEntry* const entry = find_entry(nodes, id);
  return !entry ? SERD_NOTHING : entry->node.type;
}

int
serd_nodes_compare(const SerdNodes* const nodes,
                   const SerdNodeID       lhs,
                   const SerdNodeID       rhs)
{
  if (lhs == rhs) {
    return 0;
  }

  const SerdObjectView a = serd_nodes_get_object(nodes, lhs);
  const SerdObjectView b = serd_nodes_get_object(nodes, rhs);

  int cmp = 0;
  if ((cmp = ((int)a.type - (int)b.type)) ||
      (cmp = strcmp(a.string.data, b.string.data)) ||
      (cmp = ((int)a.flags - (int)b.flags)) || !(a.flags & meta_mask)) {
    return cmp;
  }

  assert(a.flags == b.flags);
  assert(a.flags & meta_mask);
  assert(b.flags & meta_mask);
  assert(a.meta.type == b.meta.type);

  return strcmp(a.meta.string.data, b.meta.string.data);
}

bool
serd_nodes_equals_foreign_token(const SerdNodes* ZIX_NONNULL nodes,
                                const SerdNodeID             id,
                                const SerdNodes* ZIX_NONNULL other,
                                const SerdNodeID             other_id)
{
  return serd_token_view_equals(serd_nodes_get_token(nodes, id),
                                serd_nodes_get_token(other, other_id));
}

bool
serd_nodes_equals_foreign_object(const SerdNodes* ZIX_NONNULL nodes,
                                 const SerdNodeID             id,
                                 const SerdNodes* ZIX_NONNULL other,
                                 const SerdNodeID             other_id)
{
  return serd_object_view_equals(serd_nodes_get_object(nodes, id),
                                 serd_nodes_get_object(other, other_id));
}

SerdTokenView
serd_nodes_get_token(const SerdNodes* const nodes, const SerdNodeID id)
{
  assert(nodes);

  const NodesEntry* const entry = find_entry(nodes, id);
  return !entry ? serd_no_token()
                : serd_token_view(entry->node.type,
                                  serd_node_string_view(&entry->node));
}

SerdObjectView
serd_nodes_get_object(const SerdNodes* const nodes, const SerdNodeID id)
{
  assert(nodes);

  const NodesEntry* const entry = find_entry(nodes, id);
  return !entry
           ? serd_no_object()
           : serd_object_view(entry->node.type,
                              serd_node_string_view(&entry->node),
                              entry->node.flags,
                              serd_nodes_get_token(nodes, entry->node.meta));
}

SerdLiteralView
serd_nodes_get_literal(const SerdNodes* const nodes, const SerdNodeID id)
{
  assert(nodes);

  const NodesEntry* const entry = find_entry(nodes, id);
  return !entry
           ? SERD_STRUCT_LITERAL(SerdLiteralView, ZIX_STATIC_STRING(""), 0U, 0U)
           : SERD_STRUCT_LITERAL(SerdLiteralView,
                                 serd_node_string_view(&entry->node),
                                 entry->node.flags,
                                 entry->node.meta);
}
