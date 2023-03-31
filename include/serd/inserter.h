// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_INSERTER_H
#define SERD_INSERTER_H

#include "serd/attributes.h"
#include "serd/model.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "zix/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_inserter Inserter
   @ingroup serd_storage
   @{
*/

/**
   Create an inserter for writing statements to a model.

   Once created, an inserter is just a sink with no additional interface.

   @param model The model to insert received statements into.

   @param default_graph Optional default graph, which will be set on received
   statements that have no graph.  This allows, for example, loading a Turtle
   document into an isolated graph in the model.

   @return A newly allocated sink which must be freed with serd_sink_free().
*/
SERD_API SerdSink* ZIX_ALLOCATED
serd_inserter_new(SerdModel* ZIX_NONNULL       model,
                  const SerdNode* ZIX_NULLABLE default_graph);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_INSERTER_H
