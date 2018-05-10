// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world_impl.h" // IWYU pragma: keep
#include "world_internal.h"

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
    if (e->filename) {
      fprintf(stderr, "error: %s:%u:%u: ", e->filename, e->line, e->col);
    } else {
      fprintf(stderr, "error: ");
    }
    vfprintf(stderr, e->fmt, *e->args);
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

  const SerdError e = {st, NULL, 0, 0, fmt, &args_copy};
  serd_world_error(world, &e);
  va_end(args_copy);
  return st;
}

SerdStatus
serd_world_errorf(const SerdWorld* const world,
                  const SerdStatus       st,
                  const char* const      fmt,
                  ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);
  const SerdError e = {st, NULL, 0, 0, fmt, &args};
  serd_world_error(world, &e);
  va_end(args);
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
