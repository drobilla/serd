// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_TRIG_H
#define SERD_SRC_READ_TRIG_H

#include "serd/reader.h"
#include "serd/status.h"
#include "zix/attributes.h"

/**
   Read a single TriG statement.
*/
ZIX_NODISCARD SerdStatus
read_trig_statement(SerdReader* reader);

/**
   Read a complete TriG document.

   RDF 1.1 Trig: [1] trigDoc
*/
ZIX_NODISCARD SerdStatus
read_trigDoc(SerdReader* reader);

#endif // SERD_SRC_READ_TRIG_H
