// Copyright 2021-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TEST_SINK_WRITE_H
#define SERD_TEST_SINK_WRITE_H

#include "serd/node.h"
#include "serd/sink.h"

#include <assert.h>

static inline SerdStatus
serd_sink_write(const SerdSink* const         sink,
                const SerdStatementEventFlags flags,
                const SerdNode* const         subject,
                const SerdNode* const         predicate,
                const SerdNode* const         object,
                const SerdNode* const         graph)
{
  assert(sink);
  assert(subject);
  assert(predicate);
  assert(object);

  return serd_sink_write_event(
    sink,
    serd_statement_event(
      flags, serd_statement_view_nodes(subject, predicate, object, graph)));
}

#endif // SERD_TEST_SINK_WRITE_H
