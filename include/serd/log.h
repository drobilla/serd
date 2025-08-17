// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_LOG_H
#define SERD_LOG_H

#include <serd/attributes.h>
#include <serd/caret_view.h>
#include <serd/status.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_logging Logging
   @ingroup serd_errors
   @{
*/

/// Log entry level, compatible with syslog
typedef enum {
  SERD_LOG_LEVEL_EMERGENCY, ///< Emergency, system is unusable
  SERD_LOG_LEVEL_ALERT,     ///< Action must be taken immediately
  SERD_LOG_LEVEL_CRITICAL,  ///< Critical condition
  SERD_LOG_LEVEL_ERROR,     ///< Error
  SERD_LOG_LEVEL_WARNING,   ///< Warning
  SERD_LOG_LEVEL_NOTICE,    ///< Normal but significant condition
  SERD_LOG_LEVEL_INFO,      ///< Informational message
  SERD_LOG_LEVEL_DEBUG,     ///< Debug message
} SerdLogLevel;

/**
   Function for handling log messages.

   By default, the log is printed to `stderr`, but this can be overridden to
   instead send log messages to a user function of this type.

   @param handle Pointer to opaque user data.
   @param level Log level.
   @param caret Originating document location.
   @param message Log message.
*/
typedef SerdStatus (*SerdLogFunc)(void* ZIX_NULLABLE handle,
                                  SerdLogLevel       level,
                                  SerdCaretView      caret,
                                  ZixStringView      message);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_LOG_H
