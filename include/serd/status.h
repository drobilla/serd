// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATUS_H
#define SERD_STATUS_H

#include "serd/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_status Status Codes
   @ingroup serd_errors
   @{
*/

/// Return status code
typedef enum {
  SERD_SUCCESS,        ///< No error
  SERD_FAILURE,        ///< Non-fatal failure
  SERD_ERR_UNKNOWN,    ///< Unknown error
  SERD_ERR_BAD_SYNTAX, ///< Invalid syntax
  SERD_ERR_BAD_ARG,    ///< Invalid argument
  SERD_ERR_NOT_FOUND,  ///< Not found
  SERD_ERR_ID_CLASH,   ///< Encountered clashing blank node IDs
  SERD_ERR_BAD_CURIE,  ///< Invalid CURIE (e.g. prefix does not exist)
  SERD_ERR_INTERNAL,   ///< Unexpected internal error (should not happen)
  SERD_ERR_BAD_WRITE,  ///< Error writing to file/stream
  SERD_ERR_BAD_TEXT,   ///< Invalid text encoding
} SerdStatus;

/// Return a string describing a status code
SERD_CONST_API
const char* SERD_NONNULL
serd_strerror(SerdStatus status);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATUS_H
