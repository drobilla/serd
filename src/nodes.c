// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "nodes.h"

#include "memory.h"
#include "node.h"
#include "node_spec.h"

// Define the types used in the hash interface for more type safety
#define ZIX_HASH_KEY_TYPE SerdNode
#define ZIX_HASH_RECORD_TYPE NodesEntry
#define ZIX_HASH_SEARCH_DATA_TYPE NodeSpec

#include "serd/attributes.h"
#include "serd/memory.h"
#include "serd/nodes.h"
#include "serd/string_view.h"
#include "serd/uri.h"
#include "serd/value.h"
#include "zix/allocator.h"
#include "zix/digest.h"
#include "zix/hash.h"

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
   with an extra header) rather than node pointers directly.
*/
typedef struct {
  SerdAllocator  base; ///< Implementation of SerdAllocator (base "class")
  SerdAllocator* real; ///< Underlying "real" memory allocator
} SerdNodesEntryAllocator;

SERD_MALLOC_FUNC
static void*
serd_nodes_entry_aligned_alloc(SerdAllocator* const allocator,
                               const size_t         alignment,
                               const size_t         size)
{
  SerdAllocator* const real = ((SerdNodesEntryAllocator*)allocator)->real;

  void* const ptr =
    real->aligned_alloc(real, alignment, serd_node_align + size);

  return ptr ? (((uint8_t*)ptr) + serd_node_align) : NULL;
}

static void
serd_nodes_entry_aligned_free(SerdAllocator* const allocator, void* const ptr)
{
  SerdAllocator* const real = ((SerdNodesEntryAllocator*)allocator)->real;

  if (ptr) {
    real->aligned_free(real, (((uint8_t*)ptr) - serd_node_align));
  }
}

static SerdNodesEntryAllocator
serd_nodes_entry_allocator(SerdAllocator* const real)
{
  const SerdNodesEntryAllocator entry_allocator = {
    {
      NULL,
      NULL,
      NULL,
      NULL,
      serd_nodes_entry_aligned_alloc,
      serd_nodes_entry_aligned_free,
    },
    real ? real : serd_default_allocator(),
  };

  return entry_allocator;
}

struct SerdNodesImpl {
  SerdNodesEntryAllocator allocator;
  ZixHash*                hash;
};

static const StaticNode empty_static_node = {{0U, 0U, SERD_LITERAL}, {'\0'}};

static const SerdNode*
nodes_key(const NodesEntry* const entry)
{
  return &entry->node;
}

static ZixHashCode
token_hash(const ZixHashCode    seed,
           const SerdNodeType   type,
           const SerdStringView string)
{
  const SerdNode node_header = {string.len, 0U, type};
  ZixHashCode    h           = seed;

  h = zix_digest_aligned(h, &node_header, sizeof(node_header));
  h = zix_digest(h, string.buf, string.len);
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

  return h;
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

  if (node->flags & SERD_HAS_DATATYPE) {
    h = token_hash(h, SERD_URI, serd_node_string_view(serd_node_meta_c(node)));
  } else if (node->flags & SERD_HAS_LANGUAGE) {
    h = token_hash(
      h, SERD_LITERAL, serd_node_string_view(serd_node_meta_c(node)));
  }

  return h;
}

static bool
node_equals_spec(const SerdNode* const node, const NodeSpec* const spec)
{
  // Only datatype and language are relevant for equality
  static const SerdNodeFlags flag_mask = SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE;

  const SerdNodeFlags flags = spec->flags & flag_mask;

  return serd_node_type(node) == spec->type &&
         serd_node_length(node) == spec->string.len &&
         (node->flags & flag_mask) == flags &&
         !strcmp(serd_node_string_i(node), spec->string.buf) &&
         (!flags ||
          !strcmp(serd_node_string_i(serd_node_meta_c(node)), spec->meta.buf));
}

static bool
nodes_equal(const SerdNode* const a, const SerdNode* const b)
{
  static const SerdNodeFlags meta_mask =
    (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE);

  if (a == b) {
    return true;
  }

  if (a->length != b->length || a->flags != b->flags || a->type != b->type ||
      !!memcmp(serd_node_string_i(a), serd_node_string_i(b), a->length)) {
    return false;
  }

  if (a->flags & meta_mask) {
    const size_t          padded_length = serd_node_pad_length(a->length);
    const size_t          offset        = padded_length / sizeof(SerdNode);
    const SerdNode* const am            = a + 1U + offset;
    const SerdNode* const bm            = b + 1U + offset;

    return am->length == bm->length && am->type == bm->type &&
           !memcmp(serd_node_string_i(am), serd_node_string_i(bm), am->length);
  }

  return true;
}

static void
free_entry(SerdNodes* const nodes, NodesEntry* const entry)
{
  serd_aaligned_free(&nodes->allocator.base, &entry->node);
}

SerdNodes*
serd_nodes_new(SerdAllocator* const allocator)
{
  SerdNodes* const nodes =
    (SerdNodes*)serd_acalloc(allocator, 1, sizeof(SerdNodes));

  if (nodes) {
    nodes->allocator = serd_nodes_entry_allocator(allocator);

    if (!(nodes->hash = zix_hash_new(
            (ZixAllocator*)allocator, nodes_key, nodes_hash, nodes_equal))) {
      serd_afree(allocator, nodes);
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
    serd_afree(nodes->allocator.real, nodes);
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

  const ZixHashInsertPlan plan     = zix_hash_plan_insert(nodes->hash, node);
  NodesEntry* const       existing = zix_hash_record_at(nodes->hash, plan);
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
  zix_hash_insert_at(nodes->hash, plan, entry);
  return &entry->node;
}

const SerdNode*
serd_nodes_get(const SerdNodes* const nodes, const SerdNode* const node)
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

const SerdNode*
serd_nodes_token(SerdNodes* const     nodes,
                 const SerdNodeType   type,
                 const SerdStringView string)
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
  SerdAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const      node  = serd_new_token(alloc, type, string);

  return serd_nodes_manage_entry_node_at(nodes, node, plan);
}

const SerdNode*
serd_nodes_literal(SerdNodes* const     nodes,
                   const SerdStringView string,
                   const SerdNodeFlags  flags,
                   const SerdStringView meta)
{
  // Calculate a hash code for the literal without actually constructing it
  const NodeSpec    spec = literal_spec(string, flags, meta);
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
  SerdAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const      node  = serd_new_literal(alloc, string, flags, meta);

  return serd_nodes_manage_entry_node_at(nodes, node, plan);
}

const SerdNode*
serd_nodes_string(SerdNodes* const nodes, const SerdStringView string)
{
  return serd_nodes_token(nodes, SERD_LITERAL, string);
}

static const SerdNode*
try_intern(SerdNodes* const      nodes,
           const SerdWriteResult r,
           const SerdNode* const node)
{
  return r.status ? NULL : serd_nodes_intern(nodes, node);
}

const SerdNode*
serd_nodes_value(SerdNodes* const nodes, const SerdValue value)
{
  StaticNode key = empty_static_node;

  return try_intern(
    nodes, serd_node_construct_value(sizeof(key), &key, value), &key.node);
}

const SerdNode*
serd_nodes_decimal(SerdNodes* const nodes, const double value)
{
  StaticNode key = empty_static_node;

  return try_intern(
    nodes, serd_node_construct_decimal(sizeof(key), &key, value), &key.node);
}

const SerdNode*
serd_nodes_integer(SerdNodes* const nodes, const int64_t value)
{
  StaticNode key = empty_static_node;

  return try_intern(
    nodes, serd_node_construct_integer(sizeof(key), &key, value), &key.node);
}

const SerdNode*
serd_nodes_hex(SerdNodes* const  nodes,
               const void* const value,
               const size_t      value_size)
{
  assert(nodes);
  assert(value);

  /* We're more or less forced to allocate and construct an entry here, since
     we need the base64 string to hash.  Though it would be possible to
     calculate it in a streaming fashion, that would be a severe pessimisation
     in the presumably common case of raw data not being cached, since it would
     only need to be serialised again.  Keeping a tentative entry buffer around
     when possible would probably be a better improvement if this ever becomes
     a performance issue.  More ambitiously, adding support for binary nodes
     like a Real Database(TM) would largely avoid this problem. */

  SerdAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const      node  = serd_new_hex(alloc, value, value_size);

  return serd_nodes_manage_entry_node(nodes, node);
}

const SerdNode*
serd_nodes_base64(SerdNodes* const  nodes,
                  const void* const value,
                  const size_t      value_size)
{
  assert(nodes);
  assert(value);

  // Same situation as for hex above

  SerdAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const      node  = serd_new_base64(alloc, value, value_size);

  return serd_nodes_manage_entry_node(nodes, node);
}

const SerdNode*
serd_nodes_uri(SerdNodes* const nodes, const SerdStringView string)
{
  return serd_nodes_token(nodes, SERD_URI, string);
}

const SerdNode*
serd_nodes_parsed_uri(SerdNodes* const nodes, const SerdURIView uri)
{
  assert(nodes);

  /* Computing a hash for the serialised URI here would be quite complex, so,
     since this isn't expected to be a particularly hot case, we just allocate
     a new entry and try to do a normal insertion. */

  SerdAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const      node  = serd_new_parsed_uri(alloc, uri);

  return serd_nodes_manage_entry_node(nodes, node);
}

const SerdNode*
serd_nodes_file_uri(SerdNodes* const     nodes,
                    const SerdStringView path,
                    const SerdStringView hostname)
{
  assert(nodes);

  // Same situation here, not worth doing an in-place hash

  SerdAllocator* const alloc = &nodes->allocator.base;
  SerdNode* const      node  = serd_new_file_uri(alloc, path, hostname);

  return serd_nodes_manage_entry_node(nodes, node);
}

const SerdNode*
serd_nodes_blank(SerdNodes* const nodes, const SerdStringView string)
{
  return serd_nodes_token(nodes, SERD_BLANK, string);
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
