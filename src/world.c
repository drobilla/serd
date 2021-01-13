// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "log.h"
#include "node.h"

#include "serd/node.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

SerdWorld*
serd_world_new(ZixAllocator* const allocator)
{
  ZixAllocator* const actual = allocator ? allocator : zix_default_allocator();

  SerdWorld* world = (SerdWorld*)zix_calloc(actual, 1, sizeof(SerdWorld));
  SerdNode*  blank_node =
    serd_node_new(actual, serd_a_blank_string("b00000000000"));

  if (!world || !blank_node) {
    serd_node_free(actual, blank_node);
    zix_free(actual, world);
    return NULL;
  }

  world->limits.reader_stack_size = 1048576U;
  world->limits.writer_max_depth  = 128U;
  world->allocator                = actual;
  world->blank_node               = blank_node;

  serd_log_init(&world->log);

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    serd_node_free(world->allocator, world->blank_node);
    zix_free(world->allocator, world);
  }
}

const SerdNode*
serd_world_get_blank(SerdWorld* const world)
{
#define BLANK_CHARS 12

  assert(world);

  char* buf = serd_node_buffer(world->blank_node);
  memset(buf, 0, BLANK_CHARS + 1);

  world->blank_node->length =
    (size_t)snprintf(buf, BLANK_CHARS + 1, "b%u", ++world->next_blank_id);

  return world->blank_node;

#undef BLANK_CHARS
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
