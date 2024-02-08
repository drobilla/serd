// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_TURTLE_H
#define SERD_SRC_READ_TURTLE_H

#include "reader.h"

#include "serd/node.h"
#include "serd/reader.h"
#include "serd/status.h"
#include "zix/attributes.h"

#include <stdbool.h>

/**
   Read (skip) any amount of whitespace.

   RDF 1.1 Turtle: [161s] WS (but here with a *)
*/
ZIX_NODISCARD SerdStatus
read_turtle_ws_star(SerdReader* reader);

// Nonterminals

/**
   Read a Turtle prefix or base directive.

   RDF 1.1 Turtle: [4] prefixId and [5] base
*/
ZIX_NODISCARD SerdStatus
read_turtle_directive(SerdReader* reader);

/**
   Read a Turtle base directive.

   RDF 1.1 Turtle: [5] base
*/
ZIX_NODISCARD SerdStatus
read_turtle_base(SerdReader* reader, bool sparql, bool token);

/**
   Read a Turtle prefixID directive.

   RDF 1.1 Turtle: [4] prefixID
*/
ZIX_NODISCARD SerdStatus
read_turtle_prefixID(SerdReader* reader, bool sparql, bool token);

/**
   Read a Turtle IRI node.

   RDF 1.1 Turtle: [135s] iri
*/
ZIX_NODISCARD SerdStatus
read_turtle_iri(SerdReader* reader, SerdNode** dest, bool* ate_dot);

/**
   Read a Turtle subject node.

   RDF 1.1 Turtle: [10] subject
*/
ZIX_NODISCARD SerdStatus
read_turtle_subject(SerdReader* reader,
                    ReadContext ctx,
                    SerdNode**  dest,
                    int*        s_type);
/**
   Read a single Turtle "chunk" (directive or group of statements).
*/
ZIX_NODISCARD SerdStatus
read_turtle_chunk(SerdReader* reader);

/**
   Read a series of Turtle triples.

   RDF 1.1 Turtle: [6] triples
*/
ZIX_NODISCARD SerdStatus
read_turtle_triples(SerdReader* reader, ReadContext ctx, bool* ate_dot);

#endif // SERD_SRC_READ_TURTLE_H
