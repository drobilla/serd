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

#include "zix/hash.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct ZixHashEntry {
  ZixHashCode    hash;  ///< Non-folded hash value
  ZixHashRecord* value; ///< Pointer to user-owned record
} ZixHashEntry;

struct ZixHashImpl {
  ZixKeyFunc      key_func;   ///< User-provided key accessor
  ZixHashFunc     hash_func;  ///< User-provided hashing function
  ZixKeyEqualFunc equal_func; ///< User-provided equality comparison function
  size_t          count;      ///< Number of records stored in the table
  size_t          mask;       ///< Bit mask for fast modulo (n_entries - 1)
  size_t          n_entries;  ///< Power of two table size
  ZixHashEntry*   entries;    ///< Pointer to dynamically allocated table
};

static const size_t min_n_entries = 4u;
static const size_t tombstone     = 0xDEADu;

ZixHash*
zix_hash_new(const ZixKeyFunc      key_func,
             const ZixHashFunc     hash_func,
             const ZixKeyEqualFunc equal_func)
{
  ZixHash* const hash = (ZixHash*)malloc(sizeof(ZixHash));
  if (!hash) {
    return NULL;
  }

  hash->key_func   = key_func;
  hash->hash_func  = hash_func;
  hash->equal_func = equal_func;
  hash->count      = 0u;
  hash->n_entries  = min_n_entries;
  hash->mask       = hash->n_entries - 1u;

  hash->entries = (ZixHashEntry*)calloc(hash->n_entries, sizeof(ZixHashEntry));
  if (!hash->entries) {
    free(hash);
    return NULL;
  }

  return hash;
}

void
zix_hash_free(ZixHash* const hash)
{
  if (hash) {
    free(hash->entries);
    free(hash);
  }
}

ZixHashIter
zix_hash_begin(const ZixHash* const hash)
{
  return hash->entries[0u].value ? 0u : zix_hash_next(hash, 0u);
}

ZixHashIter
zix_hash_end(const ZixHash* const hash)
{
  return hash->n_entries;
}

ZixHashRecord*
zix_hash_get(const ZixHash* hash, const ZixHashIter i)
{
  assert(i < hash->n_entries);

  return hash->entries[i].value;
}

ZixHashIter
zix_hash_next(const ZixHash* const hash, ZixHashIter i)
{
  do {
    ++i;
  } while (i < hash->n_entries && !hash->entries[i].value);

  return i;
}

size_t
zix_hash_size(const ZixHash* const hash)
{
  return hash->count;
}

static inline size_t
fold_hash(const ZixHashCode h_nomod, const size_t mask)
{
  return h_nomod & mask;
}

static inline bool
is_tombstone(const ZixHashEntry* const entry)
{
  return !entry->value && entry->hash == tombstone;
}

static inline bool
is_empty(const ZixHashEntry* const entry)
{
  return !entry->value && !entry->hash;
}

static inline bool
is_match(const ZixHash* const hash,
         const ZixHashCode    code,
         const size_t         entry_index,
         ZixKeyEqualFunc      predicate,
         const void* const    user_data)
{
  const ZixHashEntry* const entry = &hash->entries[entry_index];

  return entry->value && entry->hash == code &&
         predicate(hash->key_func(entry->value), user_data);
}

static inline size_t
next_index(const ZixHash* const hash, const size_t i)
{
  return (i == hash->mask) ? 0u : (i + 1u);
}

static inline ZixHashIter
find_entry(const ZixHash* const    hash,
           const ZixHashKey* const key,
           const size_t            h,
           const ZixHashCode       code)
{
  size_t i = h;

  while (!is_match(hash, code, i, hash->equal_func, key) &&
         !is_empty(&hash->entries[i])) {
    i = next_index(hash, i);
  }

  return i;
}

static ZixStatus
rehash(ZixHash* const hash, const size_t old_n_entries)
{
  ZixHashEntry* const old_entries   = hash->entries;
  const size_t        new_n_entries = hash->n_entries;

  // Replace the array in the hash first so we can use find_entry() normally
  hash->entries = (ZixHashEntry*)calloc(new_n_entries, sizeof(ZixHashEntry));
  if (!hash->entries) {
    return ZIX_STATUS_NO_MEM;
  }

  // Reinsert every element into the new array
  for (size_t i = 0u; i < old_n_entries; ++i) {
    ZixHashEntry* const entry = &old_entries[i];

    if (entry->value) {
      assert(hash->mask == hash->n_entries - 1u);
      const size_t new_h = fold_hash(entry->hash, hash->mask);
      const size_t new_i = find_entry(hash, entry->value, new_h, entry->hash);

      hash->entries[new_i] = *entry;
    }
  }

  free(old_entries);
  return ZIX_STATUS_SUCCESS;
}

static ZixStatus
grow(ZixHash* const hash)
{
  const size_t old_n_entries = hash->n_entries;

  hash->n_entries <<= 1u;
  hash->mask = hash->n_entries - 1u;

  return rehash(hash, old_n_entries);
}

static ZixStatus
shrink(ZixHash* const hash)
{
  if (hash->n_entries > min_n_entries) {
    const size_t old_n_entries = hash->n_entries;

    hash->n_entries >>= 1u;
    hash->mask = hash->n_entries - 1u;

    return rehash(hash, old_n_entries);
  }

  return ZIX_STATUS_SUCCESS;
}

ZixHashIter
zix_hash_find(const ZixHash* const hash, const ZixHashKey* const key)
{
  const ZixHashCode h_nomod = hash->hash_func(key);
  const size_t      h       = fold_hash(h_nomod, hash->mask);
  const ZixHashIter i       = find_entry(hash, key, h, h_nomod);

  return is_empty(&hash->entries[i]) ? hash->n_entries : i;
}

ZixHashRecord*
zix_hash_find_record(const ZixHash* const hash, const ZixHashKey* const key)
{
  const ZixHashCode h_nomod = hash->hash_func(key);
  const size_t      h       = fold_hash(h_nomod, hash->mask);

  return hash->entries[find_entry(hash, key, h, h_nomod)].value;
}

ZixHashInsertPlan
zix_hash_plan_insert_prehashed(const ZixHash* const  hash,
                               const ZixHashCode     code,
                               const ZixKeyMatchFunc predicate,
                               const void* const     user_data)
{
  // Calculate an ideal initial position
  ZixHashInsertPlan pos = {code, fold_hash(code, hash->mask)};

  // Search for a free position starting at the ideal one
  size_t first_tombstone = SIZE_MAX;
  while (!is_empty(&hash->entries[pos.index])) {
    if (is_match(hash, code, pos.index, predicate, user_data)) {
      return pos;
    }

    if (first_tombstone == SIZE_MAX &&
        is_tombstone(&hash->entries[pos.index])) {
      first_tombstone = pos.index; // Remember the first/best free index
    }

    pos.index = next_index(hash, pos.index);
  }

  // If we found a tombstone before an empty slot, place the new element there
  pos.index = first_tombstone < SIZE_MAX ? first_tombstone : pos.index;
  assert(!hash->entries[pos.index].value);

  return pos;
}

ZixHashInsertPlan
zix_hash_plan_insert(const ZixHash* const hash, const ZixHashKey* const key)
{
  return zix_hash_plan_insert_prehashed(
    hash, hash->hash_func(key), hash->equal_func, key);
}

ZixHashRecord*
zix_hash_record_at(const ZixHash* const hash, const ZixHashInsertPlan position)
{
  return hash->entries[position.index].value;
}

ZixStatus
zix_hash_insert_at(ZixHash* const          hash,
                   const ZixHashInsertPlan position,
                   ZixHashRecord* const    record)
{
  if (hash->entries[position.index].value) {
    return ZIX_STATUS_EXISTS;
  }

  // Set entry to new value
  ZixHashEntry* const entry = &hash->entries[position.index];
  assert(!entry->value);
  entry->hash  = position.code;
  entry->value = record;

  // Update size and rehash if we exceeded the maximum load
  const size_t max_load = hash->n_entries / 2u + hash->n_entries / 8u;
  if (++hash->count >= max_load) {
    return grow(hash);
  }

  return ZIX_STATUS_SUCCESS;
}

ZixStatus
zix_hash_insert(ZixHash* const hash, ZixHashRecord* const record)
{
  const ZixHashKey* const key      = hash->key_func(record);
  const ZixHashInsertPlan position = zix_hash_plan_insert(hash, key);

  return zix_hash_insert_at(hash, position, record);
}

ZixStatus
zix_hash_erase(ZixHash* const        hash,
               const ZixHashIter     i,
               ZixHashRecord** const removed)
{
  // Replace entry with a tombstone
  *removed               = hash->entries[i].value;
  hash->entries[i].hash  = tombstone;
  hash->entries[i].value = NULL;

  // Decrease element count and rehash if necessary
  --hash->count;
  if (hash->count < hash->n_entries / 4u) {
    return shrink(hash);
  }

  return ZIX_STATUS_SUCCESS;
}

ZixStatus
zix_hash_remove(ZixHash* const          hash,
                const ZixHashKey* const key,
                ZixHashRecord** const   removed)
{
  const ZixHashIter i = zix_hash_find(hash, key);

  return i == hash->n_entries ? ZIX_STATUS_NOT_FOUND
                              : zix_hash_erase(hash, i, removed);
}
