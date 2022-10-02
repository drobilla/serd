// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SINK_H
#define SERD_SINK_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/status.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_sink Sink
   @ingroup serd_streaming
   @{
*/

/**
   Sink function for base URI changes.

   Called whenever the base URI of the serialisation changes.
*/
typedef SerdStatus (*SerdBaseFunc)(void* SERD_UNSPECIFIED       handle,
                                   const SerdNode* SERD_NONNULL uri);

/**
   Sink function for namespace definitions.

   Called whenever a prefix is defined in the serialisation.
*/
typedef SerdStatus (*SerdPrefixFunc)(void* SERD_UNSPECIFIED       handle,
                                     const SerdNode* SERD_NONNULL name,
                                     const SerdNode* SERD_NONNULL uri);

/**
   Sink function for statements.

   Called for every RDF statement in the serialisation.
*/
typedef SerdStatus (*SerdStatementFunc)(
  void* SERD_UNSPECIFIED        handle,
  SerdStatementFlags            flags,
  const SerdNode* SERD_NULLABLE graph,
  const SerdNode* SERD_NONNULL  subject,
  const SerdNode* SERD_NONNULL  predicate,
  const SerdNode* SERD_NONNULL  object,
  const SerdNode* SERD_NULLABLE object_datatype,
  const SerdNode* SERD_NULLABLE object_lang);

/**
   Sink function for anonymous node end markers.

   This is called to indicate that the anonymous node with the given `value`
   will no longer be referred to by any future statements (so the anonymous
   node is finished).
*/
typedef SerdStatus (*SerdEndFunc)(void* SERD_UNSPECIFIED       handle,
                                  const SerdNode* SERD_NONNULL node);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_SINK_H
