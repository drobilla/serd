// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log.h"
#include "serd_config.h"
#include "warnings.h"
#include "world_impl.h"

#include "serd/log.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#if USE_ISATTY
#  include <unistd.h>
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const log_level_strings[] = {"emergency",
                                                "alert",
                                                "critical",
                                                "error",
                                                "warning",
                                                "note",
                                                "info",
                                                "debug"};

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
    return 36; // Cyan
  case SERD_LOG_LEVEL_INFO:
  case SERD_LOG_LEVEL_DEBUG:
    break;
  }

  return 1; // White
}

static bool
terminal_supports_color(const int fd)
{
  // https://no-color.org/
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (getenv("NO_COLOR")) {
    return false;
  }

  // https://bixense.com/clicolors/
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char* const clicolor_force = getenv("CLICOLOR_FORCE");
  if (clicolor_force && !!strcmp(clicolor_force, "0")) {
    return true;
  }

  // https://bixense.com/clicolors/
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char* const clicolor = getenv("CLICOLOR");
  if (clicolor && !strcmp(clicolor, "0")) {
    return false;
  }

#if USE_ISATTY
  // Assume support if stream is a TTY (blissfully ignoring termcap nightmares)
  return isatty(fd);
#else
  (void)fd;
  return false;
#endif
}

SerdStatus
serd_log_init(SerdLog* const log)
{
  log->func         = NULL;
  log->handle       = NULL;
  log->stderr_color = terminal_supports_color(1);
  return SERD_SUCCESS;
}

SerdStatus
serd_quiet_log_func(void* const               handle,
                    const SerdLogLevel        level,
                    const size_t              n_fields,
                    const SerdLogField* const fields,
                    const ZixStringView       message)
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
  if (fields) {
    for (size_t i = 0; i < n_fields; ++i) {
      if (!strcmp(fields[i].key, key)) {
        return fields[i].value;
      }
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

  world->log.func   = log_func;
  world->log.handle = handle;
}

ZIX_LOG_FUNC(5, 6)
static int
color_printf(const bool        use_color,
             FILE* const       stream,
             const int         color,
             const bool        bold,
             const char* const fmt,
             ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)

  if (use_color) {
    fprintf(stream, "\033[0;%d%sm", color, bold ? ";1" : "");
  }

  va_start(args, fmt);
  vfprintf(stream, fmt, args);
  va_end(args);

  if (use_color) {
    fprintf(stream, "\033[0m");
  }

  return 0;
}

ZIX_LOG_FUNC(5, 0)
static SerdStatus
serd_default_vxlogf(const bool                stderr_color,
                    const SerdLogLevel        level,
                    const size_t              n_fields,
                    const SerdLogField* const fields,
                    const char* const         fmt,
                    va_list                   args)
{
  assert(fmt);

  // Print input file and position prefix if available
  const char* const file = get_log_field(n_fields, fields, "SERD_FILE");
  const char* const line = get_log_field(n_fields, fields, "SERD_LINE");
  const char* const col  = get_log_field(n_fields, fields, "SERD_COL");
  if (file && line && col) {
    color_printf(stderr_color, stderr, 1, true, "%s:%s:%s: ", file, line, col);
  } else if (file) {
    color_printf(stderr_color, stderr, 1, true, "%s: ", file);
  }

  // Print GCC-style level prefix (error, warning, etc)
  color_printf(stderr_color,
               stderr,
               level_color(level),
               true,
               "%s: ",
               log_level_strings[level]);

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
serd_vxlogf(const SerdWorld* const    world,
            const SerdLogLevel        level,
            const size_t              n_fields,
            const SerdLogField* const fields,
            const char* const         fmt,
            va_list                   args)
{
  assert(world);
  assert(fmt);

  if (world->log.func) {
    char      message[512] = {0};
    const int r            = vsnprintf(message, sizeof(message), fmt, args);

    return (r <= 0 || (size_t)r >= sizeof(message))
             ? SERD_BAD_ARG
             : world->log.func(world->log.handle,
                               level,
                               n_fields,
                               fields,
                               zix_substring(message, (size_t)r));
  }

  return serd_default_vxlogf(
    world->log.stderr_color, level, n_fields, fields, fmt, args);
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
serd_vlogf_at(const SerdWorld* ZIX_NONNULL world,
              SerdLogLevel                 level,
              const SerdCaretView*         caret,
              const char* ZIX_NONNULL      fmt,
              va_list                      args)
{
  assert(world);
  assert(fmt);

  SERD_DISABLE_NULL_WARNINGS

  const SerdNode* const document = caret ? caret->document : NULL;
  if (!document) {
    return serd_vxlogf(world, level, 0U, NULL, fmt, args);
  }

  char line[24];
  char col[24];
  snprintf(line, sizeof(line), "%u", caret->line);
  snprintf(col, sizeof(col), "%u", caret->column);

  const SerdLogField fields[] = {
    {"SERD_FILE", serd_node_string(document)},
    {"SERD_LINE", line},
    {"SERD_COL", col},
  };

  SERD_RESTORE_WARNINGS
  return serd_vxlogf(world, level, 3, fields, fmt, args);
}

SerdStatus
serd_logf_at(const SerdWorld* ZIX_NONNULL world,
             SerdLogLevel                 level,
             const SerdCaretView*         caret,
             const char* ZIX_NONNULL      fmt,
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
