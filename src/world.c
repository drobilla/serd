// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world_impl.h" // IWYU pragma: keep

#include <serd/error.h>
#include <serd/world.h>

#include <stdlib.h>

SerdWorld*
serd_world_new(void)
{
  return (SerdWorld*)calloc(1, sizeof(SerdWorld));
}

void
serd_world_free(SerdWorld* const world)
{
  free(world);
}

void
serd_world_set_error_func(SerdWorld* const world,
                          SerdLogFunc      error_func,
                          void* const      handle)
{
  world->error_func   = error_func;
  world->error_handle = handle;
}
