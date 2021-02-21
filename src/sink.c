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

#include "sink.h"

#include "statement.h"

#include "serd/serd.h"

#include <stdlib.h>

SerdSink*
serd_sink_new(void* const   handle,
              SerdEventFunc event_func,
              SerdFreeFunc  free_handle)
{
  SerdSink* sink = (SerdSink*)calloc(1, sizeof(SerdSink));

  sink->handle      = handle;
  sink->on_event    = event_func;
  sink->free_handle = free_handle;

  return sink;
}

void
serd_sink_free(SerdSink* sink)
{
  if (sink) {
    if (sink->free_handle) {
      sink->free_handle(sink->handle);
    }

    free(sink);
  }
}

SerdStatus
serd_sink_write_event(const SerdSink* sink, const SerdEvent* event)
{
  switch (event->type) {
  case SERD_BASE:
    return serd_sink_write_base(sink, event->base.uri);
  case SERD_PREFIX:
    return serd_sink_write_prefix(sink, event->prefix.name, event->prefix.uri);
  case SERD_STATEMENT:
    return serd_sink_write_statement(
      sink, event->statement.flags, event->statement.statement);
  case SERD_END:
    return serd_sink_write_end(sink, event->end.node);
  }

  return SERD_ERR_BAD_ARG;
}

SerdStatus
serd_sink_write_base(const SerdSink* sink, const SerdNode* uri)
{
  const SerdBaseEvent ev = {SERD_BASE, uri};

  return sink->on_event ? sink->on_event(sink->handle, (const SerdEvent*)&ev)
                        : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_prefix(const SerdSink* sink,
                       const SerdNode* name,
                       const SerdNode* uri)
{
  const SerdPrefixEvent ev = {SERD_PREFIX, name, uri};

  return sink->on_event ? sink->on_event(sink->handle, (const SerdEvent*)&ev)
                        : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_statement(const SerdSink*          sink,
                          const SerdStatementFlags flags,
                          const SerdStatement*     statement)
{
  const SerdStatementEvent ev = {SERD_STATEMENT, flags, statement};

  return sink->on_event ? sink->on_event(sink->handle, (const SerdEvent*)&ev)
                        : SERD_SUCCESS;
}

SerdStatus
serd_sink_write(const SerdSink*          sink,
                const SerdStatementFlags flags,
                const SerdNode*          subject,
                const SerdNode*          predicate,
                const SerdNode*          object,
                const SerdNode*          graph)
{
  const SerdStatement statement = {{subject, predicate, object, graph}, NULL};
  return serd_sink_write_statement(sink, flags, &statement);
}

SerdStatus
serd_sink_write_end(const SerdSink* sink, const SerdNode* node)
{
  const SerdEndEvent ev = {SERD_END, node};

  return sink->on_event ? sink->on_event(sink->handle, (const SerdEvent*)&ev)
                        : SERD_SUCCESS;
}
