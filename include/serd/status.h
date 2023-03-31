// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATUS_H
#define SERD_STATUS_H

#include "serd/attributes.h"
#include "zix/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_status Status Codes
   @ingroup serd_errors
   @{
*/

/// Return status code
typedef enum {
  SERD_SUCCESS,  ///< Success
  SERD_FAILURE,  ///< Non-fatal failure
  SERD_NO_DATA,  ///< Missing input
  SERD_NO_SPACE, ///< Insufficient space

  SERD_UNKNOWN_ERROR = 8, ///< Unknown error

  SERD_BAD_ALLOC,  ///< Memory allocation failed
  SERD_BAD_READ,   ///< Error reading from file
  SERD_BAD_WRITE,  ///< Error writing to file
  SERD_BAD_STREAM, ///< File or stream error
  SERD_BAD_STACK,  ///< Stack overflow
  SERD_BAD_CALL,   ///< Bad function call
  SERD_BAD_ARG,    ///< Bad function argument
  SERD_BAD_EVENT,  ///< Corrupt event in stream
  SERD_BAD_CURSOR, ///< Use of invalidated cursor
  SERD_BAD_INDEX,  ///< No suitable model index available

  SERD_BAD_SYNTAX = 32, ///< Invalid syntax

  SERD_BAD_LABEL,   ///< Clashing blank node label
  SERD_BAD_CURIE,   ///< Invalid CURIE or unknown namespace prefix
  SERD_BAD_TEXT,    ///< Invalid text encoding
  SERD_BAD_URI,     ///< Invalid or unresolved URI
  SERD_BAD_DATA,    ///< Invalid data
  SERD_BAD_LITERAL, ///< Invalid literal
  SERD_BAD_PATTERN, ///< Invalid statement pattern
} SerdStatus;

/// Return a string describing a status code
SERD_CONST_API const char* ZIX_NONNULL
serd_strerror(SerdStatus status);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATUS_H
