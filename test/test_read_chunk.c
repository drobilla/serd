/*
  Copyright 2018-2020 David Robillard <d@drobilla.net>

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

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>

static size_t n_base      = 0;
static size_t n_prefix    = 0;
static size_t n_statement = 0;
static size_t n_end       = 0;

static SerdStatus
on_base(void* handle, const SerdNode* uri)
{
  (void)handle;
  (void)uri;

  ++n_base;
  return SERD_SUCCESS;
}

static SerdStatus
on_prefix(void* handle, const SerdNode* name, const SerdNode* uri)
{
  (void)handle;
  (void)name;
  (void)uri;

  ++n_prefix;
  return SERD_SUCCESS;
}

static SerdStatus
on_statement(void* const                handle,
             const SerdStatementFlags   flags,
             const SerdStatement* const statement)
{
  (void)handle;
  (void)flags;
  (void)statement;

  ++n_statement;
  return SERD_SUCCESS;
}

static SerdStatus
on_end(void* handle, const SerdNode* node)
{
  (void)handle;
  (void)node;

  ++n_end;
  return SERD_SUCCESS;
}

static SerdStatus
on_event(void* handle, const SerdEvent* event)
{
  switch (event->type) {
  case SERD_BASE:
    return on_base(handle, event->base.uri);
  case SERD_PREFIX:
    return on_prefix(handle, event->prefix.name, event->prefix.uri);
  case SERD_STATEMENT:
    return on_statement(
      handle, event->statement.flags, event->statement.statement);
  case SERD_END:
    return on_end(handle, event->end.node);
  }

  return SERD_SUCCESS;
}

int
main(void)
{
  SerdWorld* world = serd_world_new();
  SerdSink*  sink  = serd_sink_new(NULL, on_event, NULL);

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, sink, 4096);
  assert(reader);

  assert(!serd_reader_start_string(reader,
                                   "@prefix eg: <http://example.org/> .\n"
                                   "@base <http://example.org/base> .\n"
                                   "eg:s1 eg:p1 eg:o1 ;\n"
                                   "      eg:p2 eg:o2 ,\n"
                                   "            eg:o3 .\n"
                                   "eg:s2 eg:p1 eg:o1 ;\n"
                                   "      eg:p2 eg:o2 .\n"
                                   "eg:s3 eg:p1 eg:o1 .\n"
                                   "eg:s4 eg:p1 [ eg:p3 eg:o1 ] .\n",
                                   NULL));

  assert(!serd_reader_read_chunk(reader) && n_prefix == 1);
  assert(!serd_reader_read_chunk(reader) && n_base == 1);
  assert(!serd_reader_read_chunk(reader) && n_statement == 3);
  assert(!serd_reader_read_chunk(reader) && n_statement == 5);
  assert(!serd_reader_read_chunk(reader) && n_statement == 6);
  assert(!serd_reader_read_chunk(reader) && n_statement == 8);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(n_end == 1);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);

  return 0;
}
