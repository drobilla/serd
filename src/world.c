// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "namespaces.h"
#include "node.h"
#include "serd_config.h"

#include "serd/node.h"
#include "serd/world.h"
#include "zix/string_view.h"

#if USE_FILENO && USE_ISATTY
#  include <unistd.h>
#endif

#include <assert.h>
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
serd_world_new(ZixAllocator* const allocator)
{
  ZixAllocator* const actual = allocator ? allocator : zix_default_allocator();

  SerdWorld* const world = (SerdWorld*)zix_calloc(actual, 1, sizeof(SerdWorld));

  if (!world) {
    return NULL;
  }

  SerdNodes* const nodes = serd_nodes_new(actual);
  if (!nodes) {
    zix_free(actual, world);
    return NULL;
  }

  const ZixStringView rdf_first   = zix_string(NS_RDF "first");
  const ZixStringView rdf_nil     = zix_string(NS_RDF "nil");
  const ZixStringView rdf_rest    = zix_string(NS_RDF "rest");
  const ZixStringView rdf_type    = zix_string(NS_RDF "type");
  const ZixStringView xsd_boolean = zix_string(NS_XSD "boolean");
  const ZixStringView xsd_decimal = zix_string(NS_XSD "decimal");
  const ZixStringView xsd_integer = zix_string(NS_XSD "integer");

  world->allocator = actual;
  world->nodes     = nodes;

  if (!(world->rdf_first = serd_nodes_get(nodes, serd_a_uri(rdf_first))) ||
      !(world->rdf_nil = serd_nodes_get(nodes, serd_a_uri(rdf_nil))) ||
      !(world->rdf_rest = serd_nodes_get(nodes, serd_a_uri(rdf_rest))) ||
      !(world->rdf_type = serd_nodes_get(nodes, serd_a_uri(rdf_type))) ||
      !(world->xsd_boolean = serd_nodes_get(nodes, serd_a_uri(xsd_boolean))) ||
      !(world->xsd_decimal = serd_nodes_get(nodes, serd_a_uri(xsd_decimal))) ||
      !(world->xsd_integer = serd_nodes_get(nodes, serd_a_uri(xsd_integer)))) {
    serd_nodes_free(nodes);
    zix_free(actual, world);
    return NULL;
  }

  serd_node_construct(sizeof(world->blank),
                      &world->blank,
                      serd_a_blank(zix_string("b00000000000")));

  world->stderr_color = terminal_supports_color(stderr);

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    serd_nodes_free(world->nodes);
    zix_free(world->allocator, world);
  }
}

const SerdNode*
serd_world_get_blank(SerdWorld* const world)
{
#define BLANK_CHARS 12

  assert(world);

  char* buf = world->blank.string;
  memset(buf, 0, BLANK_CHARS + 1);

  world->blank.node.length =
    (size_t)snprintf(buf, BLANK_CHARS + 1, "b%u", ++world->next_blank_id);

  return &world->blank.node;

#undef BLANK_CHARS
}

ZixAllocator*
serd_world_allocator(const SerdWorld* const world)
{
  assert(world);
  assert(world->allocator);
  return world->allocator;
}

SerdNodes*
serd_world_nodes(SerdWorld* const world)
{
  assert(world);
  return world->nodes;
}
