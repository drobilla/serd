// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world_impl.h" // IWYU pragma: keep
#include "world_internal.h"

#include <serd/caret_view.h>
#include <serd/error.h>
#include <serd/status.h>
#include <serd/world.h>
#include <zix/allocator.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

SerdStatus
serd_world_error(const SerdWorld* const world, const SerdError* const e)
{
  if (world->error_func) {
    world->error_func(world->error_handle, e);
  } else {
    fprintf(stderr, "error: ");
    if (e->caret.document.length) {
      fprintf(stderr,
              "%s:%zu:%zu: ",
              e->caret.document.data,
              e->caret.line,
              e->caret.column);
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

  const SerdError e = {st, serd_no_caret(), fmt, &args_copy};
  serd_world_error(world, &e);
  va_end(args_copy);
  return st;
}

SerdWorld*
serd_world_new(ZixAllocator* const allocator)
{
  SerdWorld* const world =
    (SerdWorld*)zix_calloc(allocator, 1, sizeof(SerdWorld));

  if (!world) {
    return NULL;
  }

  world->limits.reader_stack_size = 524288U;
  world->limits.writer_stack_size = 4096U;
  world->allocator = allocator ? allocator : zix_default_allocator();

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    zix_free(world->allocator, world);
  }
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
serd_world_set_error_func(SerdWorld* const world,
                          SerdLogFunc      error_func,
                          void* const      handle)
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
