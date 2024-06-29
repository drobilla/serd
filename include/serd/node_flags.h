// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_FLAGS_H
#define SERD_NODE_FLAGS_H

#include "serd/attributes.h"

#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_node_flags Flags
   @ingroup serd_node
   @{
*/

/// Node flags, which ORed together make a #SerdNodeFlags
typedef enum {
  SERD_IS_LONG      = 1U << 0U, ///< Literal node should be triple-quoted
  SERD_HAS_DATATYPE = 1U << 1U, ///< Literal node has datatype
  SERD_HAS_LANGUAGE = 1U << 2U, ///< Literal node has language
} SerdNodeFlag;

/// Bitwise OR of #SerdNodeFlag values
typedef uint32_t SerdNodeFlags;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_FLAGS_H
