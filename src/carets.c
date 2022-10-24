// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

// Define the types used in the ZixHash API for type safety
typedef struct CaretsEntryImpl CaretsEntry;
#define ZIX_HASH_KEY_TYPE SerdStatement
#define ZIX_HASH_RECORD_TYPE CaretsEntry

#include "carets.h"

#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <serd/struct_literal.h>
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
  SerdModelCaret                       caret;
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

ZixStatus
serd_carets_insert(ZixHash* const             carets,
                   ZixAllocator* const        allocator,
                   const SerdStatement* const statement,
                   const SerdNodeID           doc,
                   const size_t               line,
                   const size_t               column)
{
  if (!carets || !doc) {
    return ZIX_STATUS_SUCCESS;
  }

  CaretsEntry* const entry =
    (CaretsEntry*)zix_malloc(allocator, sizeof(CaretsEntry));
  if (!entry) {
    return ZIX_STATUS_NO_MEM;
  }

  entry->statement      = statement;
  entry->caret.document = doc;
  entry->caret.line     = line;
  entry->caret.column   = column;

  const ZixStatus st = zix_hash_insert(carets, entry);
  assert(!st || st == ZIX_STATUS_NO_MEM);
  if (st) {
    zix_free(allocator, entry);
  }

  return st;
}

ZixStatus
serd_carets_remove(ZixHash* const             carets,
                   ZixAllocator* const        allocator,
                   const SerdStatement* const statement)
{
  assert(statement);

  if (carets) {
    CaretsEntry*    out = NULL;
    const ZixStatus zst = zix_hash_remove(carets, statement, &out);
    assert(zst == ZIX_STATUS_SUCCESS || zst == ZIX_STATUS_NOT_FOUND);
    assert(!out || out->statement == statement);
    zix_free(allocator, out);
    (void)zst;
  }

  return ZIX_STATUS_SUCCESS;
}

SerdModelCaret
serd_carets_get(const ZixHash* const       carets,
                const SerdStatement* const statement)
{
  const ZixHashIter        i = zix_hash_find(carets, statement);
  const CaretsEntry* const e =
    (i != zix_hash_end(carets)) ? zix_hash_get(carets, i) : NULL;

  return e ? e->caret : SERD_STRUCT_LITERAL(SerdModelCaret, 0U, 0U, 0U);
}
