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

#ifndef ZIX_DIGEST_H
#define ZIX_DIGEST_H

#include "zix/common.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
   @addtogroup zix
   @{
   @name Digest
   Functions to generate a short "digest" of data with minimal collisions.

   These are good general-purpose hash functions for indexing arbitrary data,
   but are not necessarily stable across platforms and should never be used for
   cryptographic purposes.
   @{
*/

/**
   Return a 32-bit hash of a buffer.

   This can be used for any size or alignment.
*/
ZIX_PURE_API
uint32_t
zix_digest32(uint32_t seed, const void* buf, size_t len);

/**
   Return a 32-bit hash of an aligned buffer.

   Both the buffer and size must be aligned to 32 bits.  For data that fits
   these requirements, this is equivalent to, but faster than, zix_digest32().
*/
ZIX_PURE_API
uint32_t
zix_digest32_aligned(uint32_t seed, const void* buf, size_t len);

/**
   Return a 64-bit hash of a buffer.

   This can be used for any size or alignment.
*/
ZIX_PURE_API
uint64_t
zix_digest64(uint64_t seed, const void* buf, size_t len);

/**
   Return a 64-bit hash of an aligned buffer.

   Both the buffer and size must be aligned to 64 bits.  For data that fits
   these requirements, this is equivalent to, but faster than, zix_digest64().
*/
ZIX_PURE_API
uint64_t
zix_digest64_aligned(uint64_t seed, const void* buf, size_t len);

/**
   Return a pointer-sized hash of a buffer.

   This can be used for any size or alignment.

   Internally, this simply dispatches to zix_digest32() or zix_digest64() as
   appropriate.
*/
ZIX_PURE_API
size_t
zix_digest(size_t seed, const void* buf, size_t len);

/**
   Return a pointer-sized hash of an aligned buffer.

   Both the buffer and size must be aligned to the pointer size.  For data that
   fits these requirements, this is equivalent to, but faster than,
   zix_digest().

   Internally, this simply dispatches to zix_digest32_aligned() or
   zix_digest64_aligned() as appropriate.
*/
ZIX_PURE_API
size_t
zix_digest_aligned(size_t seed, const void* buf, size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZIX_DIGEST_H */
