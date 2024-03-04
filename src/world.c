// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "serd/node.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

SerdStatus
serd_world_error(const SerdWorld* const world, const SerdError* const e)
{
  if (world->error_func) {
    world->error_func(world->error_handle, e);
  } else {
    fprintf(stderr, "error: ");
    if (e->caret) {
      const SerdNode* const document = e->caret->document;
      fprintf(stderr,
              "%s:%u:%u: ",
              document ? serd_node_string(document) : "(unknown)",
              e->caret->line,
              e->caret->column);
    }
    vfprintf(stderr, e->fmt, *e->args);
    fprintf(stderr, "\n");
  }
  return e->status;
}

SerdStatus
serd_world_verrorf(const SerdWorld* const world,
                   const SerdStatus       st,
                   const char* const      fmt,
                   va_list                args)
{
  va_list args_copy;
  va_copy(args_copy, args);

  const SerdError e = {st, NULL, fmt, &args_copy};
  serd_world_error(world, &e);
  va_end(args_copy);
  return st;
}

SerdWorld*
serd_world_new(ZixAllocator* const allocator)
{
  ZixAllocator* const actual = allocator ? allocator : zix_default_allocator();

  SerdWorld* world = (SerdWorld*)zix_calloc(actual, 1, sizeof(SerdWorld));

  if (world) {
    world->limits.reader_stack_size = 1048576U;
    world->limits.writer_max_depth  = 128U;
    world->allocator                = actual;
  }

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  free(world);
}

SerdLimits
serd_world_limits(const SerdWorld* const world)
{
  assert(world);
  return world->limits;
}

SerdStatus
serd_world_set_limits(SerdWorld* const world, const SerdLimits limits)
{
  assert(world);
  world->limits = limits;
  return SERD_SUCCESS;
}

void
serd_world_set_error_func(SerdWorld*  world,
                          SerdLogFunc error_func,
                          void*       handle)
{
  world->error_func   = error_func;
  world->error_handle = handle;
}

ZixAllocator*
serd_world_allocator(const SerdWorld* const world)
{
  assert(world);
  assert(world->allocator);
  return world->allocator;
}
