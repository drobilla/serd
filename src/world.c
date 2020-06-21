/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#define _POSIX_C_SOURCE 200809L /* for posix_fadvise */

#include "world.h"

#include "cursor.h"
#include "namespaces.h"
#include "node.h"

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
    if (e->cursor) {
      fprintf(stderr,
              "%s:%u:%u: ",
              serd_node_string(e->cursor->file),
              e->cursor->line,
              e->cursor->col);
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
  SerdNodes* nodes = serd_nodes_new();

  const SerdStringView rdf_first   = SERD_STATIC_STRING(NS_RDF "first");
  const SerdStringView rdf_nil     = SERD_STATIC_STRING(NS_RDF "nil");
  const SerdStringView rdf_rest    = SERD_STATIC_STRING(NS_RDF "rest");
  const SerdStringView rdf_type    = SERD_STATIC_STRING(NS_RDF "type");
  const SerdStringView xsd_boolean = SERD_STATIC_STRING(NS_XSD "boolean");
  const SerdStringView xsd_decimal = SERD_STATIC_STRING(NS_XSD "decimal");
  const SerdStringView xsd_integer = SERD_STATIC_STRING(NS_XSD "integer");
  const SerdStringView xsd_long    = SERD_STATIC_STRING(NS_XSD "long");

  world->rdf_first   = serd_nodes_manage(nodes, serd_new_uri(rdf_first));
  world->rdf_nil     = serd_nodes_manage(nodes, serd_new_uri(rdf_nil));
  world->rdf_rest    = serd_nodes_manage(nodes, serd_new_uri(rdf_rest));
  world->rdf_type    = serd_nodes_manage(nodes, serd_new_uri(rdf_type));
  world->xsd_boolean = serd_nodes_manage(nodes, serd_new_uri(xsd_boolean));
  world->xsd_decimal = serd_nodes_manage(nodes, serd_new_uri(xsd_decimal));
  world->xsd_integer = serd_nodes_manage(nodes, serd_new_uri(xsd_integer));
  world->xsd_long    = serd_nodes_manage(nodes, serd_new_uri(xsd_long));
  world->blank_node  = serd_new_blank(SERD_STATIC_STRING("b00000000000"));
  world->nodes       = nodes;

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    serd_node_free(world->blank_node);
    serd_nodes_free(world->nodes);
    free(world);
  }
}

SerdNodes*
serd_world_nodes(SerdWorld* const world)
{
  return world->nodes;
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
