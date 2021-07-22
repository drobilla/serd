/*
  Copyright 2012-2021 David Robillard <d@drobilla.net>

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

#include "zix/digest.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#if defined(__clang__) && __clang_major__ >= 12
#  define FALLTHROUGH() __attribute__((fallthrough))
#elif defined(__GNUC__) && !defined(__clang__)
#  define FALLTHROUGH() __attribute__((fallthrough))
#else
#  define FALLTHROUGH()
#endif

/*
  64-bit hash: Essentially fasthash64, implemented here in an aligned/padded
  and a general UB-free variant.
*/

static inline uint64_t
mix64(uint64_t h)
{
  h ^= h >> 23u;
  h *= 0x2127599BF4325C37ull;
  h ^= h >> 47u;
  return h;
}

uint64_t
zix_digest64(const uint64_t seed, const void* const key, const size_t len)
{
  static const uint64_t m = 0x880355F21E6D1965ull;

  const uint64_t* const blocks   = (const uint64_t*)key;
  const size_t          n_blocks = len / sizeof(uint64_t);

  uint64_t h = seed ^ (len * m);
  for (size_t i = 0u; i < n_blocks; ++i) {
    h ^= mix64(blocks[i]);
    h *= m;
  }

  const uint8_t* const tail = (const unsigned char*)(blocks + n_blocks);
  uint64_t             v    = 0u;

  switch (len & 7u) {
  case 7:
    v |= (uint64_t)tail[6] << 48u;
    FALLTHROUGH();
  case 6:
    v |= (uint64_t)tail[5] << 40u;
    FALLTHROUGH();
  case 5:
    v |= (uint64_t)tail[4] << 32u;
    FALLTHROUGH();
  case 4:
    v |= (uint64_t)tail[3] << 24u;
    FALLTHROUGH();
  case 3:
    v |= (uint64_t)tail[2] << 16u;
    FALLTHROUGH();
  case 2:
    v |= (uint64_t)tail[1] << 8u;
    FALLTHROUGH();
  case 1:
    v |= (uint64_t)tail[0];

    h ^= mix64(v);
    h *= m;
  }

  return mix64(h);
}

uint64_t
zix_digest64_aligned(const uint64_t seed, const void* const key, size_t len)
{
  static const uint64_t m = 0x880355F21E6D1965ull;

  assert((uintptr_t)key % sizeof(uint64_t) == 0u);
  assert(len % sizeof(uint64_t) == 0u);

  const uint64_t* const blocks   = (const uint64_t*)key;
  const size_t          n_blocks = len / sizeof(uint64_t);
  uint64_t              h        = seed ^ (len * m);

  for (size_t i = 0u; i < n_blocks; ++i) {
    h ^= mix64(blocks[i]);
    h *= m;
  }

  return mix64(h);
}

/*
  32-bit hash: Essentially murmur3, reimplemented here in an aligned/padded and
  a general UB-free variant.
*/

/**
   Rotate left by some count of bits.

   This is recognized by any halfway decent compiler and compiled to a single
   instruction on architectures that have one.
*/
static inline uint32_t
rotl32(const uint32_t val, const uint32_t bits)
{
  return ((val << bits) | (val >> (32 - bits)));
}

static inline uint32_t
mix32(uint32_t h)
{
  h ^= h >> 16u;
  h *= 0x85EBCA6Bu;
  h ^= h >> 13u;
  h *= 0xC2B2AE35u;
  h ^= h >> 16u;
  return h;
}

uint32_t
zix_digest32(const uint32_t seed, const void* const key, const size_t len)
{
  static const uint32_t c1 = 0xCC9E2D51u;
  static const uint32_t c2 = 0x1B873593u;

  // Process as many 32-bit blocks as possible
  const size_t         n_blocks   = len / sizeof(uint32_t);
  const uint8_t*       data       = (const uint8_t*)key;
  const uint8_t* const blocks_end = data + (n_blocks * sizeof(uint32_t));
  uint32_t             h          = seed;
  for (; data != blocks_end; data += sizeof(uint32_t)) {
    uint32_t k = 0u;
    memcpy(&k, data, sizeof(uint32_t));

    k *= c1;
    k = rotl32(k, 15);
    k *= c2;

    h ^= k;
    h = rotl32(h, 13);
    h = h * 5u + 0xE6546B64u;
  }

  // Process any trailing bytes
  uint32_t k = 0u;
  switch (len & 3u) {
  case 3u:
    k ^= (uint32_t)data[2u] << 16u;
    FALLTHROUGH();
  case 2u:
    k ^= (uint32_t)data[1u] << 8u;
    FALLTHROUGH();
  case 1u:
    k ^= (uint32_t)data[0u];

    k *= c1;
    k = rotl32(k, 15u);
    k *= c2;
    h ^= k;
  }

  return mix32(h ^ (uint32_t)len);
}

uint32_t
zix_digest32_aligned(const uint32_t    seed,
                     const void* const key,
                     const size_t      len)
{
  static const uint32_t c1 = 0xCC9E2D51u;
  static const uint32_t c2 = 0x1B873593u;

  assert((uintptr_t)key % sizeof(uint32_t) == 0u);
  assert(len % sizeof(uint32_t) == 0u);

  const uint32_t* const blocks   = (const uint32_t*)key;
  const size_t          n_blocks = len / sizeof(uint32_t);
  uint32_t              h        = seed;
  for (size_t i = 0u; i < n_blocks; ++i) {
    uint32_t k = blocks[i];

    k *= c1;
    k = rotl32(k, 15);
    k *= c2;

    h ^= k;
    h = rotl32(h, 13);
    h = h * 5u + 0xE6546B64u;
  }

  return mix32(h ^ (uint32_t)len);
}

// Native word size wrapper

size_t
zix_digest(const size_t seed, const void* const buf, const size_t len)
{
#if UINTPTR_MAX >= UINT64_MAX
  return zix_digest64(seed, buf, len);
#else
  return zix_digest32(seed, buf, len);
#endif
}

size_t
zix_digest_aligned(const size_t seed, const void* const buf, const size_t len)
{
#if UINTPTR_MAX >= UINT64_MAX
  return zix_digest64_aligned(seed, buf, len);
#else
  return zix_digest32_aligned(seed, buf, len);
#endif
}
