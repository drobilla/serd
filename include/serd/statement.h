// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATEMENT_H
#define SERD_STATEMENT_H

#include "serd/attributes.h"

#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_statement Statements
   @ingroup serd_data
   @{
*/

/// Flags indicating inline abbreviation information for a statement
typedef enum {
  SERD_EMPTY_S = 1U << 0U, ///< Empty blank node subject
  SERD_EMPTY_O = 1U << 1U, ///< Empty blank node object
  SERD_ANON_S  = 1U << 2U, ///< Start of anonymous subject
  SERD_ANON_O  = 1U << 3U, ///< Start of anonymous object
  SERD_LIST_S  = 1U << 4U, ///< Start of list subject
  SERD_LIST_O  = 1U << 5U, ///< Start of list object
} SerdStatementFlag;

/// Bitwise OR of SerdStatementFlag values
typedef uint32_t SerdStatementFlags;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATEMENT_H
