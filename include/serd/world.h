// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_WORLD_H
#define SERD_WORLD_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_world World
   @ingroup serd_library
   @{
*/

/// Global library state
typedef struct SerdWorldImpl SerdWorld;

/// Resource limits to control allocation
typedef struct {
  size_t reader_stack_size;
  size_t writer_max_depth;
} SerdLimits;

/**
   Create a new Serd World.

   It is safe to use multiple worlds in one process, though no objects can be
   shared between worlds.
*/
SERD_MALLOC_API SerdWorld* ZIX_ALLOCATED
serd_world_new(ZixAllocator* ZIX_NULLABLE allocator);

/// Free `world`
SERD_API void
serd_world_free(SerdWorld* ZIX_NULLABLE world);

/// Return the allocator used by `world`
SERD_PURE_API ZixAllocator* ZIX_NONNULL
serd_world_allocator(const SerdWorld* ZIX_NONNULL world);

/**
   Return the current resource limits.

   These determine how much memory is allocated for reading and writing (where
   the required stack space depends on the input data.  The defaults use about
   a megabyte and over 100 levels of nesting, which is more than enough for
   most data.
*/
SERD_PURE_API SerdLimits
serd_world_limits(const SerdWorld* ZIX_NONNULL world);

/**
   Set the current resource limits.

   This updates the "current" limits, that is, those that will be used after
   this call.  It can be used to configure allocation sizes before calling some
   other function like serd_reader_new() that uses the current limits.
*/
SERD_API SerdStatus
serd_world_set_limits(SerdWorld* ZIX_NONNULL world, SerdLimits limits);

/**
   Return the nodes cache in `world`.

   The returned cache is owned by the world and contains various nodes used
   frequently by the implementation.  For convenience, it may be used to store
   additional nodes which will be freed when the world is freed.
*/
SERD_PURE_API SerdNodes* ZIX_NONNULL
serd_world_nodes(SerdWorld* ZIX_NONNULL world);

/**
   Return a unique blank node.

   The returned node is valid only until the next time serd_world_get_blank()
   is called or the world is destroyed.
*/
SERD_API const SerdNode* ZIX_NONNULL
serd_world_get_blank(SerdWorld* ZIX_NONNULL world);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_WORLD_H
