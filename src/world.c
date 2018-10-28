// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log.h"
#include "namespaces.h"
#include "node_internal.h"
#include "world_impl.h"
#include "world_internal.h"

#include "exess/exess.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

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
  static const ZixStringView rdf_first   = ZIX_STATIC_STRING(NS_RDF "first");
  static const ZixStringView rdf_nil     = ZIX_STATIC_STRING(NS_RDF "nil");
  static const ZixStringView rdf_rest    = ZIX_STATIC_STRING(NS_RDF "rest");
  static const ZixStringView rdf_type    = ZIX_STATIC_STRING(NS_RDF "type");
  static const ZixStringView xsd_boolean = ZIX_STATIC_STRING(NS_XSD "boolean");
  static const ZixStringView xsd_decimal = ZIX_STATIC_STRING(NS_XSD "decimal");
  static const ZixStringView xsd_integer = ZIX_STATIC_STRING(NS_XSD "integer");

  ZixAllocator* const actual = allocator ? allocator : zix_default_allocator();
  SerdWorld* const world = (SerdWorld*)zix_calloc(actual, 1, sizeof(SerdWorld));
  SerdNodes* const nodes = serd_nodes_new(actual);
  if (!world || !nodes) {
    serd_nodes_free(nodes);
    zix_free(actual, world);
    return NULL;
  }

  world->limits.reader_stack_size = 1048576U;
  world->limits.writer_max_depth  = 128U;
  world->allocator                = actual;
  world->nodes                    = nodes;

  serd_node_construct(sizeof(world->blank_buf),
                      world->blank_buf,
                      serd_a_blank_string("b00000000000"));

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

SerdNodes*
serd_world_nodes(SerdWorld* const world)
{
  assert(world);
  return world->nodes;
}
