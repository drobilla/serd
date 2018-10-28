// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "namespaces.h"
#include "node.h"
#include "serd_config.h"

#include "serd/node.h"
#include "serd/string_view.h"
#include "serd/world.h"

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
serd_world_new(void)
{
  SerdWorld* world = (SerdWorld*)calloc(1, sizeof(SerdWorld));
  SerdNodes* nodes = serd_nodes_new();

  const SerdStringView rdf_first   = serd_string(NS_RDF "first");
  const SerdStringView rdf_nil     = serd_string(NS_RDF "nil");
  const SerdStringView rdf_rest    = serd_string(NS_RDF "rest");
  const SerdStringView rdf_type    = serd_string(NS_RDF "type");
  const SerdStringView xsd_boolean = serd_string(NS_XSD "boolean");
  const SerdStringView xsd_decimal = serd_string(NS_XSD "decimal");
  const SerdStringView xsd_integer = serd_string(NS_XSD "integer");

  world->rdf_first   = serd_nodes_uri(nodes, rdf_first);
  world->rdf_nil     = serd_nodes_uri(nodes, rdf_nil);
  world->rdf_rest    = serd_nodes_uri(nodes, rdf_rest);
  world->rdf_type    = serd_nodes_uri(nodes, rdf_type);
  world->xsd_boolean = serd_nodes_uri(nodes, xsd_boolean);
  world->xsd_decimal = serd_nodes_uri(nodes, xsd_decimal);
  world->xsd_integer = serd_nodes_uri(nodes, xsd_integer);

  world->blank_node = serd_new_token(SERD_BLANK, serd_string("b00000000000"));
  world->nodes      = nodes;

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

  assert(world);

  char* buf = serd_node_buffer(world->blank_node);
  memset(buf, 0, BLANK_CHARS + 1);

  world->blank_node->length =
    (size_t)snprintf(buf, BLANK_CHARS + 1, "b%u", ++world->next_blank_id);

  return world->blank_node;

#undef BLANK_CHARS
}

SerdNodes*
serd_world_nodes(SerdWorld* const world)
{
  assert(world);
  return world->nodes;
}
