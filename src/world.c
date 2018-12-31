/*
  Copyright 2011-2018 David Robillard <http://drobilla.net>

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

#include "cursor.h"
#include "node.h"
#include "serd_config.h"
#include "serd_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_POSIX_FADVISE) || defined(HAVE_FILENO)
#   include <fcntl.h>
#endif

#define BLANK_CHARS 11

static const char* const log_level_strings[] = { "emergengy", "alert",
	                                             "critical",  "error",
	                                             "warning",   "note",
	                                             "info",      "debug" };

FILE*
serd_world_fopen(SerdWorld* world, const char* path, const char* mode)
{
	FILE* fd = fopen(path, mode);
	if (!fd) {
		char errno_str[24];
		snprintf(errno_str, sizeof(errno_str), "%d", errno);
		const SerdLogField fields[] = { { "ERRNO", errno_str } };
		serd_world_logf(world, "serd", SERD_LOG_LEVEL_ERR, 1, fields,
		                "failed to open file %s (%s)\n", path, strerror(errno));
		return NULL;
	}
#if defined(HAVE_POSIX_FADVISE) && defined(HAVE_FILENO)
	posix_fadvise(fileno(fd), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
	return fd;
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
serd_world_vlogf(const SerdWorld*    world,
                 const char*         domain,
                 SerdLogLevel        level,
                 const unsigned      n_fields,
                 const SerdLogField* fields,
                 const char*         fmt,
                 va_list             args)
{
	// Copy args (which may be an array) to portably get a pointer
	va_list ap;
	va_copy(ap, args);

	const SerdLogEntry e  = {domain, level, n_fields, fields, fmt, &ap};
	SerdStatus         st = SERD_SUCCESS;

	if (world->log_func) {
		st = world->log_func(world->log_handle, &e);
	} else {
		// Print GCC-style level prefix (error, warning, etc)
		fprintf(stderr, "%s: ", log_level_strings[level]);

		// Add input file and position to prefix if available
		const char* const file = serd_log_entry_get_field(&e, "SERD_FILE");
		const char* const line = serd_log_entry_get_field(&e, "SERD_LINE");
		const char* const col  = serd_log_entry_get_field(&e, "SERD_COL");
		if (file && line && col) {
			fprintf(stderr, "%s:%s:%s: ", file, line, col);
		}

		// Using a copy isn't necessary here, but it avoids a clang-tidy bug
		vfprintf(stderr, fmt, ap);
	}

	va_end(ap);
	return st;
}

SerdStatus
serd_world_logf(const SerdWorld*    world,
                const char*         domain,
                SerdLogLevel        level,
                const unsigned      n_fields,
                const SerdLogField* fields,
                const char*         fmt,
                ...)
{
	va_list args;
	va_start(args, fmt);

	const SerdStatus st =
	        serd_world_vlogf(world, domain, level, n_fields, fields, fmt, args);

	va_end(args);
	return st;
}

SerdStatus
serd_world_vlogf_internal(const SerdWorld*  world,
                          SerdStatus        st,
                          SerdLogLevel      level,
                          const SerdCursor* cursor,
                          const char*       fmt,
                          va_list           args)
{
	char st_str[8];
	snprintf(st_str, sizeof(st_str), "%d", st);
	if (cursor) {
		const char* file = serd_node_get_string(serd_cursor_get_name(cursor));

		char line[24];
		snprintf(line, sizeof(line), "%u", serd_cursor_get_line(cursor));

		char col[24];
		snprintf(col, sizeof(col), "%u", serd_cursor_get_column(cursor));

		const SerdLogField fields[] = { { "SERD_STATUS", st_str },
		                                { "SERD_FILE", file },
		                                { "SERD_LINE", line },
		                                { "SERD_COL", col } };

		serd_world_vlogf(world, "serd", level, 4, fields, fmt, args);
	} else {
		const SerdLogField fields[] = { { "SERD_STATUS", st_str } };
		serd_world_vlogf(world, "serd", level, 1, fields, fmt, args);
	}

	return st;
}

SerdStatus
serd_world_logf_internal(const SerdWorld*  world,
                         SerdStatus        st,
                         SerdLogLevel      level,
                         const SerdCursor* cursor,
                         const char*       fmt,
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

	world->rdf_first   = serd_nodes_manage(nodes, serd_new_uri(NS_RDF "first"));
	world->rdf_nil     = serd_nodes_manage(nodes, serd_new_uri(NS_RDF "nil"));
	world->rdf_rest    = serd_nodes_manage(nodes, serd_new_uri(NS_RDF "rest"));
	world->rdf_type    = serd_nodes_manage(nodes, serd_new_uri(NS_RDF "type"));
	world->xsd_boolean = serd_nodes_manage(nodes, serd_new_uri(NS_XSD "boolean"));
	world->xsd_decimal = serd_nodes_manage(nodes, serd_new_uri(NS_XSD "decimal"));
	world->xsd_integer = serd_nodes_manage(nodes, serd_new_uri(NS_XSD "integer"));

	world->blank_node = serd_new_blank("b0000000000");
	world->nodes      = nodes;

	return world;
}

void
serd_world_free(SerdWorld* world)
{
	serd_node_free(world->blank_node);
	serd_nodes_free(world->nodes);
	free(world);
}

SerdNodes*
serd_world_get_nodes(SerdWorld* world)
{
	return world->nodes;
}

const SerdNode*
serd_world_get_blank(SerdWorld* world)
{
	char* buf = serd_node_buffer(world->blank_node);
	memset(buf, 0, BLANK_CHARS + 1);
	world->blank_node->n_bytes = (size_t)snprintf(
		buf, BLANK_CHARS, "b%u", ++world->next_blank_id);
	return world->blank_node;
}

void
serd_world_set_log_func(SerdWorld* world, SerdLogFunc log_func, void* handle)
{
	world->log_func   = log_func;
	world->log_handle = handle;
}
