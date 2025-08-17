// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOOLS_CONSOLE_LOG_H
#define SERD_TOOLS_CONSOLE_LOG_H

#include <serd/log.h>
#include <serd/status.h>
#include <serd/world.h>
#include <zix/attributes.h>

#include <stdbool.h>

typedef struct {
  SerdLogLevel log_level;
  bool         stderr_color;
} SerdLog;

/// Initialize `log` and set up logging for `world`
SerdStatus
serd_log_init(SerdWorld* ZIX_NONNULL world, SerdLog* ZIX_NONNULL log);

#endif // SERD_TOOLS_CONSOLE_LOG_H
