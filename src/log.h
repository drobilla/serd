// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_LOG_H
#define SERD_SRC_LOG_H

#include "serd/caret_view.h"
#include "serd/log.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/attributes.h"

#include <stdarg.h>
#include <stdbool.h>

typedef struct {
  SerdLogFunc ZIX_NULLABLE func;
  void* ZIX_NULLABLE       handle;
  bool                     stderr_color;
} SerdLog;

SerdStatus
serd_log_init(SerdLog* ZIX_NONNULL log);

/**
   Write a simple message to the log.

   This is a convenience wrapper for serd_vxlogf() which sets no extra fields.
*/
ZIX_LOG_FUNC(3, 0)
SerdStatus
serd_vlogf(const SerdWorld* ZIX_NONNULL world,
           SerdLogLevel                 level,
           const char* ZIX_NONNULL      fmt,
           va_list                      args);

/**
   Write a simple message to the log.

   This is a convenience wrapper for serd_vlogf() that takes the format
   arguments directly.
*/
ZIX_LOG_FUNC(3, 4)
SerdStatus
serd_logf(const SerdWorld* ZIX_NONNULL world,
          SerdLogLevel                 level,
          const char* ZIX_NONNULL      fmt,
          ...);

/**
   Write a message to the log with a caret position.

   This is a convenience wrapper for serd_vxlogf() which sets `SERD_FILE`,
   `SERD_LINE`, and `SERD_COL` to the position of the given caret.  Entries are
   typically printed with a GCC-style prefix like "file.ttl:16:4".
*/
ZIX_LOG_FUNC(4, 0)
SerdStatus
serd_vlogf_at(const SerdWorld* ZIX_NONNULL      world,
              SerdLogLevel                      level,
              const SerdCaretView* ZIX_NULLABLE caret,
              const char* ZIX_NONNULL           fmt,
              va_list                           args);

/**
   Write a message to the log with a caret position.

   This is a convenience wrapper for serd_vlogf_at() that takes the format
   arguments directly.
*/
ZIX_LOG_FUNC(4, 5)
SerdStatus
serd_logf_at(const SerdWorld* ZIX_NONNULL      world,
             SerdLogLevel                      level,
             const SerdCaretView* ZIX_NULLABLE caret,
             const char* ZIX_NONNULL           fmt,
             ...);

#endif // SERD_SRC_LOG_H
