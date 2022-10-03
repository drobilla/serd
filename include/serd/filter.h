// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_FILTER_H
#define SERD_FILTER_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/world.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_filter Filter
   @ingroup serd_streaming
   @{
*/

/**
   Return a new sink that filters out statements that do not match a pattern.

   The returned sink acts like `target` in all respects, except that some
   statements may be dropped.

   @param world The world to create the sink in.

   @param target The target sink to pass the filtered data to.

   @param subject The optional subject of the filter pattern.

   @param predicate The optional predicate of the filter pattern.

   @param object The optional object of the filter pattern.

   @param graph The optional graph of the filter pattern.

   @param inclusive If true, then only statements that match the pattern are
   passed through.  Otherwise, only statements that do *not* match the pattern
   are passed through.
*/
SERD_API
SerdSink* SERD_ALLOCATED
serd_filter_new(const SerdWorld* SERD_NONNULL world,
                const SerdSink* SERD_NONNULL  target,
                const SerdNode* SERD_NULLABLE subject,
                const SerdNode* SERD_NULLABLE predicate,
                const SerdNode* SERD_NULLABLE object,
                const SerdNode* SERD_NULLABLE graph,
                bool                          inclusive);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_FILTER_H
