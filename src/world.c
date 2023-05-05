// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "caret.h"
#include "node.h"

#include "serd/node.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

SerdStatus
serd_world_error(const SerdWorld* const world, const SerdError* const e)
{
  if (world->error_func) {
    world->error_func(world->error_handle, e);
  } else {
    fprintf(stderr, "error: ");
    if (e->caret) {
      fprintf(stderr,
              "%s:%u:%u: ",
              e->caret->document ? serd_node_string(e->caret->document)
                                 : "(unknown)",
              e->caret->line,
              e->caret->col);
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
