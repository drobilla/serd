// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log.h"
#include "node_internal.h"
#include "world_impl.h"
#include "world_internal.h"

#include "exess/exess.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"

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
  ZixAllocator* const actual = allocator ? allocator : zix_default_allocator();
  SerdWorld* const world = (SerdWorld*)zix_calloc(actual, 1, sizeof(SerdWorld));
  if (!world) {
    return NULL;
  }

  world->limits.reader_stack_size = 1048576U;
  world->limits.writer_max_depth  = 128U;
  world->allocator                = actual;

  serd_node_construct(sizeof(world->blank_buf),
                      world->blank_buf,
                      serd_a_blank_string("b00000000000"));

  serd_log_init(&world->log);
  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    zix_free(world->allocator, world);
  }
}

const SerdNode*
serd_world_get_blank(SerdWorld* const world)
{
  assert(world);

  SerdNode* const blank    = (SerdNode*)world->blank_buf;
  char* const     buf      = serd_node_buffer(blank);
  const size_t    offset   = (size_t)(buf - (char*)blank);
  const size_t    buf_size = sizeof(world->blank_buf) - offset;
  size_t          i        = 0U;

  buf[i++] = 'b';
  i += exess_write_uint(++world->next_blank_id, buf_size - i, buf + i).count;
  serd_node_set_header(blank, i, 0U, SERD_BLANK);
  return blank;
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
