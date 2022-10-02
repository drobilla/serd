// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "world.h"

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/log.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/world.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int
level_color(const SerdLogLevel level)
{
  switch (level) {
  case SERD_LOG_LEVEL_EMERGENCY:
  case SERD_LOG_LEVEL_ALERT:
  case SERD_LOG_LEVEL_CRITICAL:
  case SERD_LOG_LEVEL_ERROR:
    return 31; // Red
  case SERD_LOG_LEVEL_WARNING:
    return 33; // Yellow
  case SERD_LOG_LEVEL_NOTICE:
  case SERD_LOG_LEVEL_INFO:
  case SERD_LOG_LEVEL_DEBUG:
    break;
  }

  return 1; // White
}

static void
serd_ansi_start(const bool  enabled,
                FILE* const stream,
                const int   color,
                const bool  bold)
{
  if (enabled) {
    fprintf(stream, bold ? "\033[0;%d;1m" : "\033[0;%dm", color);
  }
}

static void
serd_ansi_reset(const bool enabled, FILE* const stream)
{
  if (enabled) {
    fprintf(stream, "\033[0m");
    fflush(stream);
  }
}

static const char* const log_level_strings[] = {"emergency",
                                                "alert",
                                                "critical",
                                                "error",
                                                "warning",
                                                "note",
                                                "info",
                                                "debug"};

SerdStatus
serd_quiet_log_func(void* const               handle,
                    const SerdLogLevel        level,
                    const size_t              n_fields,
                    const SerdLogField* const fields,
                    const SerdStringView      message)
{
  (void)handle;
  (void)level;
  (void)n_fields;
  (void)fields;
  (void)message;
  return SERD_SUCCESS;
}

static const char*
get_log_field(const size_t              n_fields,
              const SerdLogField* const fields,
              const char* const         key)
{
  for (size_t i = 0; i < n_fields; ++i) {
    if (!strcmp(fields[i].key, key)) {
      return fields[i].value;
    }
  }

  return NULL;
}

void
serd_set_log_func(SerdWorld* const  world,
                  const SerdLogFunc log_func,
                  void* const       handle)
{
  assert(world);

  world->log_func   = log_func;
  world->log_handle = handle;
}

SerdStatus
serd_vxlogf(const SerdWorld* const    world,
            const SerdLogLevel        level,
            const size_t              n_fields,
            const SerdLogField* const fields,
            const char* const         fmt,
            va_list                   args)
{
  assert(world);
  assert(fmt);

  if (world->log_func) {
    char      message[512] = {0};
    const int r            = vsnprintf(message, sizeof(message), fmt, args);

    return (r <= 0 || (size_t)r >= sizeof(message))
             ? SERD_ERR_BAD_ARG
             : world->log_func(world->log_handle,
                               level,
                               n_fields,
                               fields,
                               serd_substring(message, (size_t)r));
  }

  // Print input file and position prefix if available
  const char* const file = get_log_field(n_fields, fields, "SERD_FILE");
  const char* const line = get_log_field(n_fields, fields, "SERD_LINE");
  const char* const col  = get_log_field(n_fields, fields, "SERD_COL");
  if (file) {
    serd_ansi_start(world->stderr_color, stderr, 1, true);
    if (line && col) {
      fprintf(stderr, "%s:%s:%s: ", file, line, col);
    } else {
      fprintf(stderr, "%s: ", file);
    }
    serd_ansi_reset(world->stderr_color, stderr);
  }

  // Print GCC-style level prefix (error, warning, etc)
  serd_ansi_start(world->stderr_color, stderr, level_color(level), true);
  fprintf(stderr, "%s: ", log_level_strings[level]);
  serd_ansi_reset(world->stderr_color, stderr);

  // Format and print the message itself
  vfprintf(stderr, fmt, args);

  // Print clang-tidy style check suffix
  const char* const check = get_log_field(n_fields, fields, "SERD_CHECK");
  if (check) {
    fprintf(stderr, " [%s]", check);
  }

  fprintf(stderr, "\n");
  return SERD_SUCCESS;
}

SerdStatus
serd_xlogf(const SerdWorld* const    world,
           const SerdLogLevel        level,
           const size_t              n_fields,
           const SerdLogField* const fields,
           const char* const         fmt,
           ...)
{
  assert(world);
  assert(fmt);

  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);

  const SerdStatus st = serd_vxlogf(world, level, n_fields, fields, fmt, args);

  va_end(args);
  return st;
}

SerdStatus
serd_vlogf(const SerdWorld* const world,
           const SerdLogLevel     level,
           const char* const      fmt,
           va_list                args)
{
  assert(world);
  assert(fmt);

  return serd_vxlogf(world, level, 0U, NULL, fmt, args);
}

SerdStatus
serd_logf(const SerdWorld* const world,
          const SerdLogLevel     level,
          const char* const      fmt,
          ...)
{
  assert(world);
  assert(fmt);

  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);

  const SerdStatus st = serd_vxlogf(world, level, 0U, NULL, fmt, args);

  va_end(args);
  return st;
}

SerdStatus
serd_vlogf_at(const SerdWorld* SERD_NONNULL world,
              SerdLogLevel                  level,
              const SerdCaret*              caret,
              const char* SERD_NONNULL      fmt,
              va_list                       args)
{
  assert(world);
  assert(fmt);

  const SerdNode* const document = caret ? serd_caret_document(caret) : NULL;
  if (!document) {
    return serd_vxlogf(world, level, 0U, NULL, fmt, args);
  }

  char line[24];
  char col[24];
  snprintf(line, sizeof(line), "%u", serd_caret_line(caret));
  snprintf(col, sizeof(col), "%u", serd_caret_column(caret));

  const SerdLogField fields[] = {
    {"SERD_FILE", serd_node_string(document)},
    {"SERD_LINE", line},
    {"SERD_COL", col},
  };

  return serd_vxlogf(world, level, 3, fields, fmt, args);
}

SerdStatus
serd_logf_at(const SerdWorld* SERD_NONNULL world,
             SerdLogLevel                  level,
             const SerdCaret*              caret,
             const char* SERD_NONNULL      fmt,
             ...)
{
  assert(world);
  assert(fmt);

  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);

  const SerdStatus st = serd_vlogf_at(world, level, caret, fmt, args);

  va_end(args);
  return st;
}
