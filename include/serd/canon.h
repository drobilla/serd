// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CANON_H
#define SERD_CANON_H

#include "serd/attributes.h"
#include "serd/sink.h"
#include "serd/world.h"
#include "zix/attributes.h"

#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_canon Canon
   @ingroup serd_streaming
   @{
*/

/// Flags that control canonical node transformation
typedef enum {
  SERD_CANON_LAX = 1U << 0U, ///< Tolerate and pass through invalid input
} SerdCanonFlag;

/// Bitwise OR of SerdCanonFlag values
typedef uint32_t SerdCanonFlags;

/**
   Return a new sink that transforms literals to canonical form where possible.

   The returned sink acts like `target` in all respects, except literal nodes
   in statements may be modified from the original.
*/
SERD_API SerdSink* ZIX_ALLOCATED
serd_canon_new(const SerdWorld* ZIX_NONNULL world,
               const SerdSink* ZIX_NONNULL  target,
               SerdCanonFlags               flags);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CANON_H
