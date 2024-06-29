// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_FIELD_H
#define SERD_FIELD_H

#include "serd/attributes.h"
#include "serd/node_type.h"
#include "zix/attributes.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_field Field
   @ingroup serd_data
   @{
*/

/// Index of a node in a statement
typedef enum {
  SERD_SUBJECT   = 0U, ///< Subject
  SERD_PREDICATE = 1U, ///< Predicate ("key")
  SERD_OBJECT    = 2U, ///< Object ("value")
  SERD_GRAPH     = 3U, ///< Graph ("context")
} SerdField;

/// Return true if a field can hold the given node type
SERD_CONST_API ZIX_NODISCARD bool
serd_field_supports(SerdField field, SerdNodeType type);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_FIELD_H
