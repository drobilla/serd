// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "statement.h"

#include <serd/node_id.h>
#include <zix/allocator.h>

#include <assert.h>

SerdStatement*
serd_statement_new(ZixAllocator* const allocator,
                   const SerdNodeID    s,
                   const SerdNodeID    p,
                   const SerdNodeID    o,
                   const SerdNodeID    g)
{
  assert(s);
  assert(p);
  assert(o);

  SerdStatement* statement =
    (SerdStatement*)zix_malloc(allocator, sizeof(SerdStatement));

  if (statement) {
    statement->nodes[0] = s;
    statement->nodes[1] = p;
    statement->nodes[2] = o;
    statement->nodes[3] = g;
  }

  return statement;
}

void
serd_statement_free(ZixAllocator* const  allocator,
                    SerdStatement* const statement)
{
  zix_free(allocator, statement);
}
