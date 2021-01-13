// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "node.h"
#include "serd_config.h"

#include "serd/node.h"
#include "serd/string_view.h"
#include "serd/world.h"

#if USE_FILENO && USE_ISATTY
#  include <unistd.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
terminal_supports_color(FILE* const stream)
{
  // https://no-color.org/
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (getenv("NO_COLOR")) {
    return false;
  }

  // https://bixense.com/clicolors/
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char* const clicolor_force = getenv("CLICOLOR_FORCE");
  if (clicolor_force && !!strcmp(clicolor_force, "0")) {
    return true;
  }

  // https://bixense.com/clicolors/
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char* const clicolor = getenv("CLICOLOR");
  if (clicolor && !strcmp(clicolor, "0")) {
    return false;
  }

#if USE_FILENO && USE_ISATTY

  // Assume support if stream is a TTY (blissfully ignoring termcap nightmares)
  return isatty(fileno(stream));

#else
  (void)stream;
  return false;
#endif
}

SerdWorld*
serd_world_new(void)
{
  SerdWorld* world = (SerdWorld*)calloc(1, sizeof(SerdWorld));

  world->blank_node = serd_new_blank(serd_string("b00000000000"));

  world->stderr_color = terminal_supports_color(stderr);

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
#define BLANK_CHARS 12

  char* buf = serd_node_buffer(world->blank_node);
  memset(buf, 0, BLANK_CHARS + 1);

  world->blank_node->length =
    (size_t)snprintf(buf, BLANK_CHARS + 1, "b%u", ++world->next_blank_id);

  return world->blank_node;

#undef BLANK_CHARS
}
