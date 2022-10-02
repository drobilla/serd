// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_WORLD_H
#define SERD_WORLD_H

#include "serd/attributes.h"
#include "serd/error.h"
#include "serd/node.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_world World
   @ingroup serd
   @{
*/

/// Global library state
typedef struct SerdWorldImpl SerdWorld;

/**
   Create a new Serd World.

   It is safe to use multiple worlds in one process, though no objects can be
   shared between worlds.
*/
SERD_MALLOC_API
SerdWorld* SERD_ALLOCATED
serd_world_new(void);

/// Free `world`
SERD_API
void
serd_world_free(SerdWorld* SERD_NULLABLE world);

/**
   Return a unique blank node.

   The returned node is valid only until the next time serd_world_get_blank()
   is called or the world is destroyed.
*/
SERD_API
const SerdNode* SERD_NONNULL
serd_world_get_blank(SerdWorld* SERD_NONNULL world);

/**
   Set a function to be called when errors occur.

   The `error_func` will be called with `handle` as its first argument.  If
   no error function is set, errors are printed to stderr.
*/
SERD_API
void
serd_world_set_error_func(SerdWorld* SERD_NONNULL     world,
                          SerdErrorFunc SERD_NULLABLE error_func,
                          void* SERD_NULLABLE         handle);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_WORLD_H
