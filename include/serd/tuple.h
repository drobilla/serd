// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TUPLE_H
#define SERD_TUPLE_H

#include <serd/attributes.h>
#include <serd/node_id.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_tuple Tuple
   @ingroup serd_data
   @{
*/

/// A tuple of node IDs
typedef struct {
  SerdNodeID nodes[4]; ///< Subject, predicate, object, graph
} SerdTuple;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_TUPLE_H
