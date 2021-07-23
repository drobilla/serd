// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "log.h"
#include "namespaces.h"
#include "node.h"

#include "serd/node.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

SerdWorld*
serd_world_new(ZixAllocator* const allocator)
{
  ZixAllocator* const actual = allocator ? allocator : zix_default_allocator();

  SerdWorld* const world = (SerdWorld*)zix_calloc(actual, 1, sizeof(SerdWorld));
  SerdNodes* const nodes = serd_nodes_new(actual);
  if (!world || !nodes) {
    serd_nodes_free(nodes);
    zix_free(actual, world);
    return NULL;
  }

  static const ZixStringView rdf_first   = ZIX_STATIC_STRING(NS_RDF "first");
  static const ZixStringView rdf_nil     = ZIX_STATIC_STRING(NS_RDF "nil");
  static const ZixStringView rdf_rest    = ZIX_STATIC_STRING(NS_RDF "rest");
  static const ZixStringView rdf_type    = ZIX_STATIC_STRING(NS_RDF "type");
  static const ZixStringView xsd_boolean = ZIX_STATIC_STRING(NS_XSD "boolean");
  static const ZixStringView xsd_decimal = ZIX_STATIC_STRING(NS_XSD "decimal");
  static const ZixStringView xsd_integer = ZIX_STATIC_STRING(NS_XSD "integer");

  world->limits.reader_stack_size = 1048576U;
  world->limits.writer_max_depth  = 128U;
  world->allocator                = actual;
  world->nodes                    = nodes;

  serd_log_init(&world->log);

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

  serd_node_construct(
    sizeof(world->blank), &world->blank, serd_a_blank_string("b00000000000"));

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

SerdNodes*
serd_world_nodes(SerdWorld* const world)
{
  assert(world);
  return world->nodes;
}
