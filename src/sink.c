// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sink.h"

#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/statement_view.h"
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
  assert(sink);
  sink->base = base_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_prefix_func(SerdSink* sink, SerdPrefixFunc prefix_func)
{
  assert(sink);
  sink->prefix = prefix_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_statement_func(SerdSink* sink, SerdStatementFunc statement_func)
{
  assert(sink);
  sink->statement = statement_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_end_func(SerdSink* sink, SerdEndFunc end_func)
{
  assert(sink);
  sink->end = end_func;
  return SERD_SUCCESS;
}

SerdStatus
serd_sink_write_base(const SerdSink* sink, const SerdNode* uri)
{
  assert(sink);
  assert(uri);
  return sink->base ? sink->base(sink->handle, uri) : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_prefix(const SerdSink* sink,
                       const SerdNode* name,
                       const SerdNode* uri)
{
  assert(sink);
  assert(name);
  assert(uri);
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

  const SerdStatementView statement = {subject, predicate, object, graph};

  return sink->statement ? sink->statement(sink->handle, flags, statement)
                         : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_end(const SerdSink* sink, const SerdNode* node)
{
  assert(sink);
  assert(node);
  return sink->end ? sink->end(sink->handle, node) : SERD_SUCCESS;
}
