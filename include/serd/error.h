// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ERROR_H
#define SERD_ERROR_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/status.h"

#include <stdarg.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_error Error reporting
   @ingroup serd_errors
   @{
*/

/// An error description
typedef struct {
  SerdStatus                    status; ///< Error code
  const SerdCaret* ZIX_NULLABLE caret;  ///< File origin of error
  const char* ZIX_NONNULL       fmt;    ///< Printf-style format string
  va_list* ZIX_NONNULL          args;   ///< Arguments for fmt
} SerdError;

/**
   Callback function for errors.

   @param handle Handle for user data.
   @param error Error description.
*/
typedef SerdStatus (*SerdErrorFunc)(void* ZIX_NULLABLE           handle,
                                    const SerdError* ZIX_NONNULL error);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_ERROR_H
