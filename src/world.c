// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "serd/world.h"

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
serd_world_set_error_func(SerdWorld*  world,
                          SerdLogFunc error_func,
                          void*       handle)
{
  world->error_func   = error_func;
  world->error_handle = handle;
}
