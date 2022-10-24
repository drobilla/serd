// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_INSERTER_H
#define SERD_INSERTER_H

#include <serd/attributes.h>
#include <serd/env.h>
#include <serd/handler.h>
#include <serd/model.h>
#include <serd/token_view.h>
#include <zix/attributes.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_inserter Inserter
   @ingroup serd_storage
   @{
*/

/**
   Create an inserter for writing statements to a model.

   Once created, an inserter is just a handler with no additional interface.

   @param model The model to insert received statements into.

   @param env Environment used to expand nodes before insertion.

   @param default_graph Optional default graph, which will be set on received
   statements that have no graph.  This allows, for example, loading a Turtle
   document into an isolated graph in the model.

   @return A newly allocated handler which must be freed with
   serd_handler_free().
*/
SERD_API SerdHandler* ZIX_ALLOCATED
serd_inserter_new(SerdModel* ZIX_NONNULL     model,
                  const SerdEnv* ZIX_NONNULL env,
                  SerdTokenView              default_graph);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_INSERTER_H
