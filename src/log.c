// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log_internal.h"
#include "world_impl.h"

#include <serd/caret_view.h>
#include <serd/log.h>
#include <serd/status.h>
#include <serd/world.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

SerdStatus
serd_default_log_func(void* const         handle,
                      const SerdLogLevel  level,
                      const SerdCaretView caret,
                      const ZixStringView message)
{
  (void)handle;

  static const char* const level_strings[] = {"emergency",
                                              "alert",
                                              "critical",
                                              "error",
                                              "warning",
                                              "note",
                                              "info",
                                              "debug"};

  const int rc =
    (caret.document.length || caret.line || caret.column)
      ? fprintf(stderr,
                "%s:%zu:%zu: %s: %s\n",
                caret.document.data,
                caret.line,
                caret.column,
                level_strings[level],
                message.data)
      : fprintf(stderr, "%s: %s\n", level_strings[level], message.data);

  return rc > 0 ? SERD_SUCCESS : SERD_BAD_WRITE;
}

SerdStatus
serd_vlogf(const SerdWorld* const world,
           const SerdLogLevel     level,
           const SerdCaretView    caret,
           const char* const      fmt,
           va_list                args)
{
  assert(world);
  assert(fmt);

  if (level > world->log_level) {
    return SERD_SUCCESS;
  }

  char      msg[512] = {0};
  const int r        = vsnprintf(msg, sizeof(msg), fmt, args);

  return (r <= 0 || (size_t)r >= sizeof(msg))
           ? SERD_BAD_ARG
           : world->log.func(
               world->log.handle, level, caret, zix_substring(msg, (size_t)r));
}

SerdStatus
serd_logf(const SerdWorld* ZIX_NONNULL world,
          const SerdLogLevel           level,
          const SerdCaretView          caret,
          const char* ZIX_NONNULL      fmt,
          ...)
{
  assert(world);
  assert(fmt);

  if (level > world->log_level) {
    return SERD_SUCCESS;
  }

  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);

  const SerdStatus st = serd_vlogf(world, level, caret, fmt, args);

  va_end(args);
  return st;
}
