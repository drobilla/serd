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

#include "serd_internal.h"

#include <stdarg.h>

#include "world.h"

SerdStatus
serd_world_error(const SerdWorld* world, const SerdError* e)
{
	if (world->error_sink) {
		world->error_sink(world->error_handle, e);
	} else {
		fprintf(stderr, "error: %s:%u:%u: ", e->filename, e->line, e->col);
		vfprintf(stderr, e->fmt, *e->args);
	}
	return e->status;
}

SerdStatus
serd_world_errorf(const SerdWorld* world, SerdStatus st, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const SerdError e = { st, NULL, 0, 0, fmt, &args };
	serd_world_error(world, &e);
	va_end(args);
	return st;
}

SerdWorld*
serd_world_new(void)
{
	return (SerdWorld*)calloc(1, sizeof(SerdWorld));
}

void
serd_world_free(SerdWorld* world)
{
	free(world);
}

void
serd_world_set_error_sink(SerdWorld*    world,
                          SerdErrorSink error_sink,
                          void*         handle)
{
	world->error_sink   = error_sink;
	world->error_handle = handle;
}
