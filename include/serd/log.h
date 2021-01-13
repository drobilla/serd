// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_LOG_H
#define SERD_LOG_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/world.h"

#include <stdarg.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_logging Logging
   @ingroup serd
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
   A structured log field.

   Fields are used to add metadata to log messages.  Syslog-compatible keys
   should be used where possible, otherwise, keys should be namespaced to
   prevent clashes.

   Serd itself uses the following keys:

   - ERRNO - The `errno` of the original system error if any (decimal string)
   - SERD_COL - The 1-based column number in the file (decimal string)
   - SERD_FILE - The file which caused this message (string)
   - SERD_LINE - The 1-based line number in the file (decimal string)
   - SERD_CHECK - The check/warning/etc that triggered this message (string)
*/
typedef struct {
  const char* SERD_NONNULL key;   ///< Field name
  const char* SERD_NONNULL value; ///< Field value
} SerdLogField;

/**
   Function for handling log messages.

   By default, the log is printed to `stderr`.  This can be overridden by
   passing a function of this type to serd_set_log_func().

   @param handle Pointer to opaque user data.
   @param level Log level.
   @param n_fields Number of entries in `fields`.
   @param fields An array of `n_fields` extra log fields.
   @param message Log message.
*/
typedef SerdStatus (*SerdLogFunc)(void* SERD_NULLABLE               handle,
                                  SerdLogLevel                      level,
                                  size_t                            n_fields,
                                  const SerdLogField* SERD_NULLABLE fields,
                                  SerdStringView                    message);

/// A #SerdLogFunc that does nothing (for suppressing log output)
SERD_CONST_API
SerdStatus
serd_quiet_log_func(void* SERD_NULLABLE               handle,
                    SerdLogLevel                      level,
                    size_t                            n_fields,
                    const SerdLogField* SERD_NULLABLE fields,
                    SerdStringView                    message);

/**
   Set a function to be called with log messages (typically errors).

   If no custom logging function is set, then messages are printed to stderr.

   @param world World that will send log entries to the given function.

   @param log_func Log function to call for every log message.  Each call to
   this function represents a complete log message with an implicit trailing
   newline.

   @param handle Opaque handle that will be passed to every invocation of
   `log_func`.
*/
SERD_API
void
serd_set_log_func(SerdWorld* SERD_NONNULL   world,
                  SerdLogFunc SERD_NULLABLE log_func,
                  void* SERD_NULLABLE       handle);

/**
   Write a message to the log with a `va_list`.

   This is the fundamental and most powerful function for writing entries to
   the log, the others are convenience wrappers that ultimately call this.

   This writes a single complete entry to the log, and so may not be used to
   print parts of a line like a more general printf-like function.  There
   should be no trailing newline in `fmt`.  Arguments following `fmt` should
   correspond to conversion specifiers in the format string as in printf from
   the standard C library.

   @param world World to log to.
   @param level Log level.
   @param n_fields Number of entries in `fields`.
   @param fields An array of `n_fields` extra log fields.
   @param fmt Format string.
   @param args Arguments for `fmt`.

   @return A status code, which is always #SERD_SUCCESS with the default log
   function.  If a custom log function is set with serd_set_log_func() and it
   returns an error, then that error is returned here.
*/
SERD_API
SERD_LOG_FUNC(5, 0)
SerdStatus
serd_vxlogf(const SerdWorld* SERD_NONNULL     world,
            SerdLogLevel                      level,
            size_t                            n_fields,
            const SerdLogField* SERD_NULLABLE fields,
            const char* SERD_NONNULL          fmt,
            va_list                           args);

/**
   Write a message to the log with extra fields.

   This is a convenience wrapper for serd_vxlogf() that takes the format
   arguments directly.
*/
SERD_API
SERD_LOG_FUNC(5, 6)
SerdStatus
serd_xlogf(const SerdWorld* SERD_NONNULL     world,
           SerdLogLevel                      level,
           size_t                            n_fields,
           const SerdLogField* SERD_NULLABLE fields,
           const char* SERD_NONNULL          fmt,
           ...);

/**
   Write a simple message to the log.

   This is a convenience wrapper for serd_vxlogf() which sets no extra fields.
*/
SERD_API
SERD_LOG_FUNC(3, 0)
SerdStatus
serd_vlogf(const SerdWorld* SERD_NONNULL world,
           SerdLogLevel                  level,
           const char* SERD_NONNULL      fmt,
           va_list                       args);

/**
   Write a simple message to the log.

   This is a convenience wrapper for serd_vlogf() that takes the format
   arguments directly.
*/
SERD_API
SERD_LOG_FUNC(3, 4)
SerdStatus
serd_logf(const SerdWorld* SERD_NONNULL world,
          SerdLogLevel                  level,
          const char* SERD_NONNULL      fmt,
          ...);

/**
   Write a message to the log with a caret position.

   This is a convenience wrapper for serd_vxlogf() which sets `SERD_FILE`,
   `SERD_LINE`, and `SERD_COL` to the position of the given caret.  Entries are
   typically printed with a GCC-style prefix like "file.ttl:16:4".
*/
SERD_API
SERD_LOG_FUNC(4, 0)
SerdStatus
serd_vlogf_at(const SerdWorld* SERD_NONNULL  world,
              SerdLogLevel                   level,
              const SerdCaret* SERD_NULLABLE caret,
              const char* SERD_NONNULL       fmt,
              va_list                        args);

/**
   Write a message to the log with a caret position.

   This is a convenience wrapper for serd_vlogf_at() that takes the format
   arguments directly.
*/
SERD_API
SERD_LOG_FUNC(4, 5)
SerdStatus
serd_logf_at(const SerdWorld* SERD_NONNULL  world,
             SerdLogLevel                   level,
             const SerdCaret* SERD_NULLABLE caret,
             const char* SERD_NONNULL       fmt,
             ...);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_LOG_H
