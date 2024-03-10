// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sink_impl.h"

#include "serd/caret_view.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stddef.h>

SerdSink*
serd_sink_new(ZixAllocator* const allocator,
              void* const         handle,
              SerdEventFunc       event_func,
              SerdFreeFunc        free_handle)
{
  SerdSink* sink = (SerdSink*)zix_calloc(allocator, 1, sizeof(SerdSink));

  if (sink) {
    sink->allocator   = allocator;
    sink->handle      = handle;
    sink->on_event    = event_func;
    sink->free_handle = free_handle;
  }

  return sink;
}

void
serd_sink_free(SerdSink* sink)
{
  if (sink) {
    if (sink->free_handle) {
      sink->free_handle(sink->handle);
    }

    zix_free(sink->allocator, sink);
  }
}

SerdStatus
serd_sink_write_event(const SerdSink* sink, const SerdEvent* event)
{
  assert(sink);
  assert(event);
  return sink->on_event ? sink->on_event(sink->handle, event) : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_base(const SerdSink* sink, const SerdNode* uri)
{
  assert(sink);
  assert(uri);

  const SerdBaseEvent ev = {SERD_BASE, uri};

  return sink->on_event ? sink->on_event(sink->handle, (const SerdEvent*)&ev)
                        : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_prefix(const SerdSink* sink,
                       const SerdNode* name,
                       const SerdNode* uri)
{
  assert(sink);
  assert(name);
  assert(uri);

  const SerdPrefixEvent ev = {SERD_PREFIX, name, uri};

  return sink->on_event ? sink->on_event(sink->handle, (const SerdEvent*)&ev)
                        : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_statement(const SerdSink*               sink,
                          const SerdStatementEventFlags flags,
                          const SerdStatementView       statement)
{
  static const SerdCaretView no_caret = {NULL, 0, 0};

  return serd_sink_write_statement_from(sink, flags, statement, no_caret);
}

SerdStatus
serd_sink_write_statement_from(const SerdSink*               sink,
                               const SerdStatementEventFlags flags,
                               const SerdStatementView       statement,
                               const SerdCaretView           caret)
{
  assert(sink);
  assert(statement.subject);
  assert(statement.predicate);
  assert(statement.object);

  const SerdStatementEvent statement_ev = {
    SERD_STATEMENT, flags, statement, caret};

  SerdEvent ev = {SERD_STATEMENT};
  ev.statement = statement_ev;

  return sink->on_event ? sink->on_event(sink->handle, &ev) : SERD_SUCCESS;
}

SerdStatus
serd_sink_write(const SerdSink*               sink,
                const SerdStatementEventFlags flags,
                const SerdNode*               subject,
                const SerdNode*               predicate,
                const SerdNode*               object,
                const SerdNode*               graph)
{
  assert(sink);
  assert(subject);
  assert(predicate);
  assert(object);

  const SerdStatementView statement = {subject, predicate, object, graph};

  return serd_sink_write_statement(sink, flags, statement);
}

SerdStatus
serd_sink_write_end(const SerdSink* sink, const SerdNode* node)
{
  assert(sink);
  assert(node);

  const SerdEndEvent ev = {SERD_END, node};

  return sink->on_event ? sink->on_event(sink->handle, (const SerdEvent*)&ev)
                        : SERD_SUCCESS;
}
