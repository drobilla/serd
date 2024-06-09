// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "nodes.h"

#include "node_internal.h"
#include "node_spec.h"
#include "warnings.h"

// Define the types used in the hash interface for more type safety
#define ZIX_HASH_KEY_TYPE SerdNode
#define ZIX_HASH_RECORD_TYPE NodesEntry
#define ZIX_HASH_SEARCH_DATA_TYPE NodeSpec

#include "serd/nodes.h"
#include "serd/stream_result.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/digest.h"
#include "zix/hash.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if ((defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112l) || \
     (defined(__cplusplus) && __cplusplus >= 201103L))

static_assert(sizeof(NodesEntryHead) == sizeof(SerdNode),
              "NodesEntryHead must be the same size as SerdNode for alignment");

#endif

/*
  The main goal here is to make getting an existing node as fast as possible,
  so that this can be used as a convenient node cache with minimal performance
  overhead.  Mainly this means striving to avoid allocation when potentially
  inserting a new node.

  This is achieved by generating a hash code from node components without
  actually allocating that node, and using the low-level insertion interface of
  ZixHash to do a custom search.  This way, an entry is only allocated when
  necessary, and the hash table is only searched once.

  The downside is that subtle and very bad things will happen if the hash
  generated for the node does not match the actual hash of the node.  The
  exessive number of assertions around this are there to provide some defense
  against such mistakes.  Cave operatur.
*/

/**
   The maximum size of a node that will be made statically on the stack.

   This mostly applies to things like numeric literal nodes, where a small
   maximum size is exploited to avoid allocation.  The largest static node
   string is the longest xsd:decimal, which is 327 bytes.  We need a bit more
   space than that here for the node header, padding, and datatype.
*/
#define MAX_STATIC_NODE_SIZE 384

typedef struct {
  SerdNode node;
  char     body[MAX_STATIC_NODE_SIZE];
} StaticNode;

/**
   Allocator for allocating entries in the node hash table.

   This allocator implements only the methods used by the serd_node_*
   constructors, and transparently increases the allocation size so there is
   room for an extra NodesEntryHead at the start.  This allows the serd_node_*
   constructors to be re-used here, even though the table stores entries (nodes
-   with an extra header) rather than node pointers directly.
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

  void* const ptr = real->calloc(real, nmemb, sizeof(NodesEntryHead) + size);

  return ptr ? (((uint8_t*)ptr) + sizeof(NodesEntryHead)) : NULL;
}

static void
serd_nodes_entry_free(ZixAllocator* const allocator, void* const ptr)
{
  ZixAllocator* const real = ((SerdNodesEntryAllocator*)allocator)->real;

  if (ptr) {
    real->free(real, (((uint8_t*)ptr) - sizeof(NodesEntryHead)));
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
  SerdNodesEntryAllocator allocator;
  ZixHash*                hash;
};

static const SerdNodeFlags meta_mask = (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE);

static const SerdNode*
nodes_key(const NodesEntry* const entry)
{
  return &entry->node;
}

static ZixHashCode
token_hash(const ZixHashCode   seed,
           const SerdNodeType  type,
           const ZixStringView string)
{
  const SerdNode node_header = {NULL, string.length, 0U, type};
  ZixHashCode    h           = seed;

  h = zix_digest_aligned(h, &node_header, sizeof(node_header));
  h = zix_digest(h, string.data, string.length);
  return h;
}

static ZixHashCode
spec_hash(const NodeSpec spec)
{
  ZixHashCode h = token_hash(0U, spec.type, spec.string);

  if (spec.flags & SERD_HAS_DATATYPE) {
    h = token_hash(h, SERD_URI, spec.meta);
  } else if (spec.flags & SERD_HAS_LANGUAGE) {
    h = token_hash(h, SERD_LITERAL, spec.meta);
  }

  return zix_digest(h, &spec.flags, sizeof(spec.flags));
}

static ZixHashCode
nodes_hash(const SerdNode* const node)
{
  /* If you're thinking "I know, I'll make this faster by just hashing the
     entire node in one call!", think harder.  Since zero bytes affect the hash
     value, that would mean that the hash code for the node won't match the one
     for the node spec, and the above-mentioned subtle and very bad things will
     happen.  This function deliberately mirrors spec_hash() above to make it
     relatively easy to see that the two will match.

     It would be more elegant to construct a spec from the node and then hash
     that with the exact same code used for searching, but there's currently no
     use for that anywhere else and this way is a bit faster. */

  ZixHashCode h = token_hash(0U, node->type, serd_node_string_view(node));

  const SerdNode* const datatype = serd_node_datatype(node);
  if (datatype) {
    h = token_hash(h, SERD_URI, serd_node_string_view(datatype));
  } else {
    const SerdNode* const language = serd_node_language(node);
    if (language) {
      h = token_hash(h, SERD_LITERAL, serd_node_string_view(language));
    }
  }

  return zix_digest(h, &node->flags, sizeof(node->flags));
}

static bool
node_equals_spec(const SerdNode* const node, const NodeSpec* const spec)
{
  return serd_node_type(node) == spec->type &&
         serd_node_length(node) == spec->string.length &&
         node->flags == spec->flags &&
         !strcmp(serd_node_string(node), spec->string.data) &&
         (!(node->flags & meta_mask) ||
          !strcmp(serd_node_string(serd_node_meta(node)), spec->meta.data));
}

ZIX_PURE_FUNC static bool
nodes_meta_equal(const SerdNode* const a, const SerdNode* const b)
{
  assert(a->flags & meta_mask);
  assert(b->flags & meta_mask);

  const SerdNode* const am = a->meta;
  const SerdNode* const bm = b->meta;

  return am->length == bm->length && am->type == bm->type &&
         !memcmp(serd_node_string(am), serd_node_string(bm), am->length);
}

ZIX_PURE_FUNC static bool
nodes_equal(const SerdNode* const a, const SerdNode* const b)
{
  return (a == b) ||
         (a->length == b->length && a->flags == b->flags &&
          a->type == b->type &&
          !memcmp(serd_node_string(a), serd_node_string(b), a->length) &&
          (!(a->flags & meta_mask) || nodes_meta_equal(a, b)));
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

    if (!(nodes->hash =
            zix_hash_new(allocator, nodes_key, nodes_hash, nodes_equal))) {
      zix_free(allocator, nodes);
      return NULL;
    }
  }

  return nodes;
}

void
serd_nodes_free(SerdNodes* nodes)
{
  if (nodes) {
    for (ZixHashIter i = zix_hash_begin(nodes->hash);
         i != zix_hash_end(nodes->hash);
         i = zix_hash_next(nodes->hash, i)) {
      free_entry(nodes, (NodesEntry*)zix_hash_get(nodes->hash, i));
    }

    zix_hash_free(nodes->hash);
    zix_free(nodes->allocator.real, nodes);
  }
}

size_t
serd_nodes_size(const SerdNodes* nodes)
{
  assert(nodes);

  return zix_hash_size(nodes->hash);
}

const SerdNode*
serd_nodes_intern(SerdNodes* nodes, const SerdNode* node)
{
  assert(nodes);
  if (!node) {
    return NULL;
  }

  SERD_DISABLE_NULL_WARNINGS
  const ZixHashInsertPlan plan     = zix_hash_plan_insert(nodes->hash, node);
  NodesEntry* const       existing = zix_hash_record_at(nodes->hash, plan);
  SERD_RESTORE_WARNINGS
  if (existing) {
    assert(serd_node_equals(&existing->node, node));
    ++existing->head.refs;
    return &existing->node;
  }

  SerdNode* const new_node = serd_node_copy(&nodes->allocator.base, node);
  if (!new_node) {
    return NULL;
  }

  NodesEntry* const entry = (NodesEntry*)(new_node - 1U);

  entry->head.refs = 1U;

  // Insert the entry (blissfully ignoring a failed hash size increase)
  if (zix_hash_insert_at(nodes->hash, plan, entry)) {
    free_entry(nodes, entry);
    return NULL;
  }

  return &entry->node;
}

const SerdNode*
serd_nodes_existing(const SerdNodes* const nodes, const SerdNode* const node)
{
  assert(nodes);

  if (!node) {
    return NULL;
  }

  NodesEntry* const entry = zix_hash_find_record(nodes->hash, node);

  return entry ? &entry->node : NULL;
}

static const SerdNode*
serd_nodes_manage_entry_at(SerdNodes* const        nodes,
                           NodesEntry* const       entry,
                           const ZixHashInsertPlan plan)
{
  assert(nodes);
  assert(entry);

  entry->head.refs = 1U;

  // Insert the entry (blissfully ignoring a failed hash size increase)
  if (zix_hash_insert_at(nodes->hash, plan, entry)) {
    free_entry(nodes, entry);
    return NULL;
  }

  return &entry->node;
}

static const SerdNode*
serd_nodes_manage_entry_node_at(SerdNodes* const        nodes,
                                SerdNode* const         node,
                                const ZixHashInsertPlan plan)
{
  if (!node) {
    return NULL;
  }

  NodesEntry* const entry = (NodesEntry*)(node - 1U);

  return serd_nodes_manage_entry_at(nodes, entry, plan);
}

static const SerdNode*
serd_nodes_manage_entry_node(SerdNodes* const nodes, SerdNode* const node)
{
  if (!node) {
    return NULL;
  }

  NodesEntry* const       entry    = (NodesEntry*)(node - 1U);
  const ZixHashInsertPlan plan     = zix_hash_plan_insert(nodes->hash, node);
  NodesEntry* const       existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    assert(serd_node_equals(&existing->node, node));
    assert(nodes_hash(&existing->node) == plan.code);
    ++existing->head.refs;
    free_entry(nodes, entry);
    return &existing->node;
  }

  assert(nodes_hash(&entry->node) == plan.code);

  return serd_nodes_manage_entry_at(nodes, entry, plan);
}

static const SerdNode*
serd_nodes_token(SerdNodes* const    nodes,
                 const SerdNodeType  type,
                 const ZixStringView string)
{
  // Calculate a hash code for the token without actually constructing it
  const NodeSpec    key  = token_spec(type, string);
  const ZixHashCode code = spec_hash(key);

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_spec, &key);

  // If we found an existing node, bump its reference count and return it
  NodesEntry* const existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    assert(nodes_hash(&existing->node) == code);
    ++existing->head.refs;
    return &existing->node;
  }

  // Otherwise, allocate and manage a new one
  ZixAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const     node  = serd_node_new(alloc, serd_a_token(type, string));

  return serd_nodes_manage_entry_node_at(nodes, node, plan);
}

static const SerdNode*
serd_nodes_literal(SerdNodes* const      nodes,
                   const ZixStringView   string,
                   const SerdNodeFlags   flags,
                   const SerdNode* const meta)
{
  // Calculate a hash code for the literal without actually constructing it
  const NodeSpec spec = literal_spec(
    string, flags, meta ? serd_node_string_view(meta) : zix_empty_string());
  const ZixHashCode code = spec_hash(spec);

  // Find an insert position in the hash table
  const ZixHashInsertPlan plan =
    zix_hash_plan_insert_prehashed(nodes->hash, code, node_equals_spec, &spec);

  // If we found an existing node, bump its reference count and return it
  NodesEntry* const existing = zix_hash_record_at(nodes->hash, plan);
  if (existing) {
    assert(nodes_hash(&existing->node) == code);
    ++existing->head.refs;
    return &existing->node;
  }

  // Otherwise, allocate and manage a new one
  ZixAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const     node =
    serd_node_new(alloc, serd_a_literal(string, flags, meta));

  return serd_nodes_manage_entry_node_at(nodes, node, plan);
}

static const SerdNode*
try_intern(SerdNodes* const       nodes,
           const SerdStreamResult r,
           const SerdNode* const  node)
{
  return r.status ? NULL : serd_nodes_intern(nodes, node);
}

const SerdNode*
serd_nodes_get(SerdNodes* const nodes, const SerdNodeArgs args)
{
  StaticNode key = {{NULL, 0U, 0U, SERD_LITERAL}, {'\0'}};

  /* Some types here are cleverly hashed without allocating a node, but others
     simply allocate a new node and attempt to manage it.  It would be possible
     to calculate an in-place hash for some of them, but this is quite
     complicated and error-prone, so more difficult edge cases aren't yet
     implemented. */

  switch (args.type) {
  case SERD_NODE_ARGS_TOKEN:
    return serd_nodes_token(
      nodes, args.data.as_token.type, args.data.as_token.string);

  case SERD_NODE_ARGS_PARSED_URI:
  case SERD_NODE_ARGS_FILE_URI:
  case SERD_NODE_ARGS_PREFIXED_NAME:
  case SERD_NODE_ARGS_JOINED_URI:
    break;

  case SERD_NODE_ARGS_LITERAL:
    return serd_nodes_literal(nodes,
                              args.data.as_literal.string,
                              args.data.as_literal.flags,
                              args.data.as_literal.meta);

  case SERD_NODE_ARGS_PRIMITIVE:
  case SERD_NODE_ARGS_DECIMAL:
  case SERD_NODE_ARGS_INTEGER:
    return try_intern(
      nodes, serd_node_construct(sizeof(key), &key, args), &key.node);

  case SERD_NODE_ARGS_HEX:
  case SERD_NODE_ARGS_BASE64:
    break;
  }

  return serd_nodes_manage_entry_node(
    nodes, serd_node_new(&nodes->allocator.base, args));
}

void
serd_nodes_deref(SerdNodes* const nodes, const SerdNode* const node)
{
  if (!node) {
    return;
  }

  ZixHashIter i = zix_hash_find(nodes->hash, node);
  if (i == zix_hash_end(nodes->hash)) {
    return;
  }

  NodesEntry* const entry = zix_hash_get(nodes->hash, i);
  if (--entry->head.refs == 0U) {
    NodesEntry* removed = NULL;
    zix_hash_erase(nodes->hash, i, &removed);
    assert(removed == entry);
    free_entry(nodes, removed);
  }
}
