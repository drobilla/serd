// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_READ_NQUADS_H
#define SERD_READ_NQUADS_H

#include "serd/serd.h"

// Nonterminals

/**
   Read a complete NQuads document.

   RDF 1.1 NQuads: [1] nquadsDoc
*/
SerdStatus
read_nquadsDoc(SerdReader* reader);

#endif // SERD_READ_NQUADS_H
