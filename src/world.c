// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "caret.h"
#include "node.h"

#include "serd/caret.h"
#include "serd/node.h"

#include "serd/string_view.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLANK_CHARS 12

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
              serd_node_string(e->caret->file),
              e->caret->line,
              e->caret->col);
    }
    vfprintf(stderr, e->fmt, *e->args);
  }
  return e->status;
}

SerdStatus
serd_world_errorf(const SerdWorld* const world,
                  const SerdStatus       st,
                  const char* const      fmt,
                  ...)
{
  va_list args;
  va_start(args, fmt);
  const SerdError e = {st, NULL, fmt, &args};
  serd_world_error(world, &e);
  va_end(args);
  return st;
}

SerdWorld*
serd_world_new(void)
{
  SerdWorld* world = (SerdWorld*)calloc(1, sizeof(SerdWorld));

  world->blank_node = serd_new_blank(serd_string("b00000000000"));

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    serd_node_free(world->blank_node);
    free(world);
  }
}

const SerdNode*
serd_world_get_blank(SerdWorld* const world)
{
  char* buf = serd_node_buffer(world->blank_node);
  memset(buf, 0, BLANK_CHARS + 1);

  world->blank_node->length =
    (size_t)snprintf(buf, BLANK_CHARS + 1, "b%u", ++world->next_blank_id);

  return world->blank_node;
}

void
serd_world_set_error_func(SerdWorld*    world,
                          SerdErrorFunc error_func,
                          void*         handle)
{
  world->error_func   = error_func;
  world->error_handle = handle;
}
