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

#ifndef SERD_WORLD_H
#define SERD_WORLD_H

#include "serd/serd.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

struct SerdWorldImpl {
	SerdNodes*      nodes;
	SerdLogFunc     log_func;
	void*           log_handle;
	SerdNode*       blank_node;
	const SerdNode* rdf_first;
	const SerdNode* rdf_nil;
	const SerdNode* rdf_rest;
	const SerdNode* rdf_type;
	const SerdNode* xsd_boolean;
	const SerdNode* xsd_decimal;
	const SerdNode* xsd_integer;
	uint32_t        next_blank_id;
};

FILE* serd_world_fopen(SerdWorld* world, const char* path, const char* mode);

/// Write a message to the log
SERD_API
SERD_LOG_FUNC(5, 0)
SerdStatus
serd_world_vlogf_internal(const SerdWorld*  world,
                          SerdStatus        st,
                          SerdLogLevel      level,
                          const SerdCursor* cursor,
                          const char*       fmt,
                          va_list           args);

/// Write a message to the log
SERD_API
SERD_LOG_FUNC(5, 6)
SerdStatus
serd_world_logf_internal(const SerdWorld*  world,
                         SerdStatus        st,
                         SerdLogLevel      level,
                         const SerdCursor* cursor,
                         const char*       fmt,
                         ...);

#define SERD_LOG_ERRORF(world, st, fmt, ...) \
	serd_world_logf_internal(world, st, SERD_LOG_LEVEL_ERR, NULL, fmt, __VA_ARGS__);

#define SERD_LOG_ERROR(world, st, msg) \
	serd_world_logf_internal(world, st, SERD_LOG_LEVEL_ERR, NULL, msg);

#endif  // SERD_WORLD_H
