// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NO_DEFAULT_CONFIG

// POSIX.1-2001: isatty()
#  ifndef HAVE_ISATTY
#    if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
#      define HAVE_ISATTY 1
#    endif
#  endif

#endif // !defined(SERD_NO_DEFAULT_CONFIG)

#if defined(HAVE_ISATTY) && HAVE_ISATTY
#  define USE_ISATTY 1
#else
#  define USE_ISATTY 0
#endif

#include "console_log.h"

#include <serd/caret_view.h>
#include <serd/log.h>
#include <serd/status.h>
#include <serd/world.h>
#include <zix/string_view.h>

#if USE_ISATTY
#  include <unistd.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { N_LEVELS = 8U };

static const char* const log_level_strings[N_LEVELS] = {"emergency",
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
  //                                    R   R   R   R   Y   C  W  W
  static const int colors[N_LEVELS] = {31, 31, 31, 31, 33, 36, 1, 1};
  return colors[(unsigned)level];
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

static SerdStatus
serd_default_log_func(void* const         handle,
                      const SerdLogLevel  level,
                      const SerdCaretView caret,
                      const ZixStringView message)
{
  const SerdLog* const log = (const SerdLog*)handle;

  if (log->stderr_color) { // Set bold white foreground
    fprintf(stderr, "\033[0;1;1m");
  }

  // Print input file and position prefix if available
  if (caret.document.length) {
    fprintf(stderr, "%s:", caret.document.data);
  }
  if (caret.line) {
    fprintf(stderr, "%zu:%zu:", caret.line, caret.column);
  }

  if (log->stderr_color) {
    fprintf(stderr, "\033[0;%d;1m", level_color(level)); // Set level color
  }

  // Print GCC-style level prefix (error, warning, etc)
  const bool wrote_prefix = caret.document.length || caret.line || caret.column;
  fprintf(stderr, "%s%s: ", wrote_prefix ? " " : "", log_level_strings[level]);

  if (log->stderr_color) {
    fprintf(stderr, "\033[0m"); // Reset text
  }

  // Format and print the message itself followed by a newline
  fprintf(stderr, "%s\n", message.data);
  return SERD_SUCCESS;
}

SerdStatus
serd_log_init(SerdWorld* const world, SerdLog* const log)
{
  log->stderr_color = terminal_supports_color(2);
  serd_world_set_log_func(world, serd_default_log_func, log);
  return SERD_SUCCESS;
}
