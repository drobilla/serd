// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_LOG_INTERNAL_H
#define SERD_SRC_LOG_INTERNAL_H

#include <serd/caret_view.h>
#include <serd/log.h>
#include <serd/status.h>
#include <serd/world.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stdarg.h>

typedef struct {
  SerdLogFunc ZIX_NULLABLE func;
  void* ZIX_NULLABLE       handle;
} SerdLog;

/// Write a basic formatted log message from va_list arguments with a caret
ZIX_LOG_FUNC(4, 0)
SerdStatus
serd_vlogf(const SerdWorld* ZIX_NONNULL world,
           SerdLogLevel                 level,
           SerdCaretView                caret,
           const char* ZIX_NONNULL      fmt,
           va_list                      args);

/// Write a message to the log with a caret position
ZIX_LOG_FUNC(4, 5)
SerdStatus
serd_logf(const SerdWorld* ZIX_NONNULL world,
          SerdLogLevel                 level,
          SerdCaretView                caret,
          const char* ZIX_NONNULL      fmt,
          ...);

/// Default log function that simply prints to stderr
SerdStatus
serd_default_log_func(void* ZIX_UNSPECIFIED handle,
                      SerdLogLevel          level,
                      SerdCaretView         caret,
                      ZixStringView         message);

#endif // SERD_SRC_LOG_INTERNAL_H
