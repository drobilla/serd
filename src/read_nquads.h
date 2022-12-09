// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_NQUADS_H
#define SERD_SRC_READ_NQUADS_H

#include "serd/reader.h"
#include "serd/status.h"

// Nonterminals

/**
   Read a single NQuads line.

   May read a statement, but may also just skip some input like comments or
   extra whitespace.
*/
SerdStatus
read_nquads_line(SerdReader* reader);

/**
   Read a complete NQuads document.

   RDF 1.1 NQuads: [1] nquadsDoc
*/
SerdStatus
read_nquadsDoc(SerdReader* reader);

#endif // SERD_SRC_READ_NQUADS_H
