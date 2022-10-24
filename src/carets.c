// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

// Define the types used in the ZixHash API for type safety
typedef struct CaretsEntryImpl CaretsEntry;
#define ZIX_HASH_KEY_TYPE SerdStatement
#define ZIX_HASH_RECORD_TYPE CaretsEntry

#include "carets.h"

#include <serd/caret_view.h>
#include <serd/node_args.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/status.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/digest.h>
#include <zix/hash.h>
#include <zix/status.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

struct CaretsEntryImpl {
  const SerdStatement* ZIX_UNSPECIFIED statement;
  SerdNodeID                           document;
  size_t                               line;
  size_t                               col;
};

static const SerdStatement*
carets_key(const CaretsEntry* const entry)
{
  return entry->statement;
}

static ZixHashCode
carets_hash(const SerdStatement* const statement)
{
  return zix_digest_aligned(0U, &statement, sizeof(SerdStatement*));
}

ZIX_PURE_FUNC static bool
carets_equal(const SerdStatement* const a, const SerdStatement* const b)
{
  return a == b;
}

ZixHash*
serd_carets_new(ZixAllocator* const allocator)
{
  return zix_hash_new(allocator, carets_key, carets_hash, carets_equal);
}

void
serd_carets_free(ZixHash* const carets, ZixAllocator* const allocator)
{
  if (carets) {
    for (ZixHashIter i = zix_hash_begin(carets); i != zix_hash_end(carets);
         i             = zix_hash_next(carets, i)) {
      zix_free(allocator, zix_hash_get(carets, i));
    }

    zix_hash_free(carets);
  }
}

SerdStatus
serd_carets_insert(ZixHash* const             carets,
                   ZixAllocator* const        allocator,
                   SerdNodes* const           nodes,
                   const SerdStatement* const statement,
                   const SerdCaretView        caret)
{
  if (!caret.document.length) {
    return SERD_SUCCESS;
  }

  const SerdNodeID doc = serd_nodes_id(nodes, serd_a_string(caret.document));
  if (!doc) {
    return SERD_BAD_ALLOC;
  }

  CaretsEntry* const entry =
    (CaretsEntry*)zix_malloc(allocator, sizeof(CaretsEntry));
  if (!entry) {
    return SERD_BAD_ALLOC;
  }

  entry->statement = statement;
  entry->document  = doc;
  entry->line      = caret.line;
  entry->col       = caret.column;

  const ZixStatus st = zix_hash_insert(carets, entry);
  assert(!st || st == ZIX_STATUS_NO_MEM);
  if (st) {
    zix_free(allocator, entry);
    return SERD_BAD_ALLOC;
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_carets_remove(ZixHash* const             carets,
                   ZixAllocator* const        allocator,
                   const SerdStatement* const statement)
{
  assert(statement);

  CaretsEntry*    removed = NULL;
  const ZixStatus st      = zix_hash_remove(carets, statement, &removed);
  assert(st == ZIX_STATUS_SUCCESS || st == ZIX_STATUS_NOT_FOUND);
  assert(!removed || removed->statement == statement);
  zix_free(allocator, removed);
  (void)st;
  return SERD_SUCCESS;
}

SerdCaretView
serd_carets_get(const ZixHash* const       carets,
                const SerdNodes* const     nodes,
                const SerdStatement* const statement)
{
  SerdCaretView view = {{"", 0U}, 0U, 0U};

  const ZixHashIter        i = zix_hash_find(carets, statement);
  const CaretsEntry* const caret =
    (i != zix_hash_end(carets)) ? zix_hash_get(carets, i) : NULL;

  if (caret) {
    assert(caret);
    assert(caret->document);
    view.document = serd_nodes_get_token(nodes, caret->document).string;
    view.line     = caret->line;
    view.column   = caret->col;
  }

  return view;
}
