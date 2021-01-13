/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#define _POSIX_C_SOURCE 200809L /* for posix_fadvise */

#include "world.h"

#include "namespaces.h"
#include "node.h"
#include "serd_config.h"

#if USE_FILENO && USE_ISATTY
#  include <unistd.h>
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLANK_CHARS 12

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

static bool
terminal_supports_color(FILE* const stream)
{
  // https://no-color.org/
  if (!getenv("NO_COLOR")) {
    // https://bixense.com/clicolors/
    const char* const clicolor_force = getenv("CLICOLOR_FORCE");
    if (clicolor_force && strcmp(clicolor_force, "0")) {
      return true;
    }

    // https://bixense.com/clicolors/
    const char* const clicolor = getenv("CLICOLOR");
    if (clicolor && !strcmp(clicolor, "0")) {
      return false;
    }

#if USE_FILENO && USE_ISATTY
    // Assume support if TERM contains "color" (doing this properly is hard...)
    const char* const term = getenv("TERM");
    return isatty(fileno(stream)) && term && strstr(term, "color");
#else
    (void)stream;
#endif
  }

  return false;
}

static bool
serd_ansi_start(FILE* const stream, const int color, const bool bold)
{
  if (terminal_supports_color(stream)) {
    return fprintf(stream, bold ? "\033[0;%d;1m" : "\033[0;%dm", color);
  }

  return 0;
}

static void
serd_ansi_reset(FILE* const stream)
{
  if (terminal_supports_color(stream)) {
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

SERD_CONST_FUNC
SerdStatus
serd_quiet_error_func(void* const handle, const SerdLogEntry* const entry)
{
  (void)handle;
  (void)entry;
  return SERD_SUCCESS;
}

static const char*
get_log_field(const SerdLogField* const fields,
              const size_t              n_fields,
              const char* const         key)
{
  for (size_t i = 0; i < n_fields; ++i) {
    if (!strcmp(fields[i].key, key)) {
      return fields[i].value;
    }
  }

  return NULL;
}

const char*
serd_log_entry_get_field(const SerdLogEntry* const entry, const char* const key)
{
  return get_log_field(entry->fields, entry->n_fields, key);
}

SerdStatus
serd_world_vlogf(const SerdWorld* const    world,
                 const SerdLogLevel        level,
                 const size_t              n_fields,
                 const SerdLogField* const fields,
                 const char* const         fmt,
                 va_list                   args)
{
  // Copy args (which may be an array) to portably get a pointer
  va_list ap;
  va_copy(ap, args);

  const SerdLogEntry e  = {fields, fmt, &ap, level, n_fields};
  SerdStatus         st = SERD_SUCCESS;

  if (world->log_func) {
    st = world->log_func(world->log_handle, &e);
  } else {
    // Print input file and position prefix if available
    const char* const file = serd_log_entry_get_field(&e, "SERD_FILE");
    const char* const line = serd_log_entry_get_field(&e, "SERD_LINE");
    const char* const col  = serd_log_entry_get_field(&e, "SERD_COL");
    if (file && line && col) {
      serd_ansi_start(stderr, 1, true);
      fprintf(stderr, "%s:%s:%s: ", file, line, col);
      serd_ansi_reset(stderr);
    }

    // Print GCC-style level prefix (error, warning, etc)
    serd_ansi_start(stderr, level_color(level), true);
    fprintf(stderr, "%s: ", log_level_strings[level]);
    serd_ansi_reset(stderr);

    // Using a copy isn't necessary here, but it avoids a clang-tidy bug
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
  }

  va_end(ap);
  return st;
}

SerdStatus
serd_world_logf(const SerdWorld* const    world,
                const SerdLogLevel        level,
                const size_t              n_fields,
                const SerdLogField* const fields,
                const char* const         fmt,
                ...)
{
  va_list args;
  va_start(args, fmt);

  const SerdStatus st =
    serd_world_vlogf(world, level, n_fields, fields, fmt, args);

  va_end(args);
  return st;
}

SerdStatus
serd_world_vlogf_internal(const SerdWorld* const  world,
                          const SerdStatus        st,
                          const SerdLogLevel      level,
                          const SerdCursor* const cursor,
                          const char* const       fmt,
                          va_list                 args)
{
  char st_str[12];
  snprintf(st_str, sizeof(st_str), "%u", st);
  if (cursor) {
    const char* file = serd_node_string(serd_cursor_name(cursor));

    char line[24];
    snprintf(line, sizeof(line), "%u", serd_cursor_line(cursor));

    char col[24];
    snprintf(col, sizeof(col), "%u", serd_cursor_column(cursor));

    const SerdLogField fields[] = {{"SERD_STATUS", st_str},
                                   {"SERD_FILE", file},
                                   {"SERD_LINE", line},
                                   {"SERD_COL", col}};

    serd_world_vlogf(world, level, 4, fields, fmt, args);
  } else {
    const SerdLogField fields[] = {{"SERD_STATUS", st_str}};
    serd_world_vlogf(world, level, 1, fields, fmt, args);
  }

  return st;
}

SerdStatus
serd_world_logf_internal(const SerdWorld* const  world,
                         const SerdStatus        st,
                         const SerdLogLevel      level,
                         const SerdCursor* const cursor,
                         const char* const       fmt,
                         ...)
{
  va_list args;
  va_start(args, fmt);
  serd_world_vlogf_internal(world, st, level, cursor, fmt, args);
  va_end(args);
  return st;
}

SerdWorld*
serd_world_new(void)
{
  SerdWorld* world = (SerdWorld*)calloc(1, sizeof(SerdWorld));
  SerdNodes* nodes = serd_nodes_new();

  const SerdStringView rdf_first   = SERD_STATIC_STRING(NS_RDF "first");
  const SerdStringView rdf_nil     = SERD_STATIC_STRING(NS_RDF "nil");
  const SerdStringView rdf_rest    = SERD_STATIC_STRING(NS_RDF "rest");
  const SerdStringView rdf_type    = SERD_STATIC_STRING(NS_RDF "type");
  const SerdStringView xsd_boolean = SERD_STATIC_STRING(NS_XSD "boolean");
  const SerdStringView xsd_decimal = SERD_STATIC_STRING(NS_XSD "decimal");
  const SerdStringView xsd_integer = SERD_STATIC_STRING(NS_XSD "integer");
  const SerdStringView xsd_long    = SERD_STATIC_STRING(NS_XSD "long");

  world->rdf_first   = serd_nodes_manage(nodes, serd_new_uri(rdf_first));
  world->rdf_nil     = serd_nodes_manage(nodes, serd_new_uri(rdf_nil));
  world->rdf_rest    = serd_nodes_manage(nodes, serd_new_uri(rdf_rest));
  world->rdf_type    = serd_nodes_manage(nodes, serd_new_uri(rdf_type));
  world->xsd_boolean = serd_nodes_manage(nodes, serd_new_uri(xsd_boolean));
  world->xsd_decimal = serd_nodes_manage(nodes, serd_new_uri(xsd_decimal));
  world->xsd_integer = serd_nodes_manage(nodes, serd_new_uri(xsd_integer));
  world->xsd_long    = serd_nodes_manage(nodes, serd_new_uri(xsd_long));
  world->blank_node  = serd_new_blank(SERD_STATIC_STRING("b00000000000"));
  world->nodes       = nodes;

  return world;
}

void
serd_world_free(SerdWorld* const world)
{
  if (world) {
    serd_node_free(world->blank_node);
    serd_nodes_free(world->nodes);
    free(world);
  }
}

SerdNodes*
serd_world_nodes(SerdWorld* const world)
{
  return world->nodes;
}

const SerdNode*
serd_world_get_blank(SerdWorld* const world)
{
  char* buf = serd_node_buffer(world->blank_node);
  memset(buf, 0, BLANK_CHARS + 1);

  world->blank_node->length =
    (size_t)snprintf(buf, BLANK_CHARS + 1, "b%u", ++world->next_blank_id);

  return world->blank_node;
}

void
serd_world_set_log_func(SerdWorld* const  world,
                        const SerdLogFunc log_func,
                        void* const       handle)
{
  world->log_func   = log_func;
  world->log_handle = handle;
}
