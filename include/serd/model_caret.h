// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_MODEL_CARET_H
#define SERD_MODEL_CARET_H

#include <serd/attributes.h>
#include <serd/node_id.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_model_caret Model Caret
   @ingroup serd_storage
   @{
*/

/// The origin of a statement in a model
typedef struct {
  SerdNodeID document; ///< Document node ID (typically a plain literal)
  size_t     line;     ///< 1-based line within document
  size_t     column;   ///< 1-based column within line
} SerdModelCaret;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_MODEL_CARET_H
