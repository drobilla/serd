// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_FIELD_H
#define SERD_FIELD_H

#include "serd/attributes.h"

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

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_FIELD_H
