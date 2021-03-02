// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sink.h"

#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"

#include <assert.h>
#include <stdlib.h>

SerdSink*
serd_sink_new(void* handle, SerdFreeFunc free_handle)
{
  SerdSink* sink = (SerdSink*)calloc(1, sizeof(SerdSink));

  sink->handle      = handle;
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
serd_sink_set_base_func(SerdSink* sink, SerdBaseFunc base_func)
{
  sink->base = base_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_prefix_func(SerdSink* sink, SerdPrefixFunc prefix_func)
{
  sink->prefix = prefix_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_statement_func(SerdSink* sink, SerdStatementFunc statement_func)
{
  sink->statement = statement_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_end_func(SerdSink* sink, SerdEndFunc end_func)
{
  sink->end = end_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_write_base(const SerdSink* sink, const SerdNode* uri)
{
  return sink->base ? sink->base(sink->handle, uri) : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_prefix(const SerdSink* sink,
                       const SerdNode* name,
                       const SerdNode* uri)
{
  return sink->prefix ? sink->prefix(sink->handle, name, uri) : SERD_SUCCESS;
}

SerdStatus
serd_sink_write(const SerdSink*          sink,
                const SerdStatementFlags flags,
                const SerdNode*          subject,
                const SerdNode*          predicate,
                const SerdNode*          object,
                const SerdNode*          graph)
{
  assert(sink);
  assert(subject);
  assert(predicate);
  assert(object);

  return sink->statement
           ? sink->statement(
               sink->handle, flags, graph, subject, predicate, object)
           : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_end(const SerdSink* sink, const SerdNode* node)
{
  return sink->end ? sink->end(sink->handle, node) : SERD_SUCCESS;
}
