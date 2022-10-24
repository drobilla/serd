// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log_internal.h"
#include "world_impl.h" // IWYU pragma: keep
#include "world_internal.h"

#include <serd/attributes.h>
#include <serd/log.h>
#include <serd/status.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

uint32_t
serd_world_next_document_id(SerdWorld* const world)
{
  return ++world->next_document_id;
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
  world->log_level = SERD_LOG_LEVEL_WARNING;
  world->log.func  = serd_default_log_func;

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    zix_free(world->allocator, world);
  }
}

SerdStatus
serd_world_set_log_level(SerdWorld* const world, const SerdLogLevel level)
{
  assert(world);

  world->log_level = level;
  return SERD_SUCCESS;
}

void
serd_world_set_log_func(SerdWorld* const  world,
                        const SerdLogFunc log_func,
                        void* const       handle)
{
  assert(world);

  world->log.func   = log_func ? log_func : serd_default_log_func;
  world->log.handle = handle;
}

SERD_API SerdStatus
serd_world_reset_counters(SerdWorld* ZIX_NONNULL world)
{
  world->next_document_id = 0;
  return SERD_SUCCESS;
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

ZixAllocator*
serd_world_allocator(const SerdWorld* const world)
{
  assert(world);
  assert(world->allocator);
  return world->allocator;
}
