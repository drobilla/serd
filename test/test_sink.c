// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define NS_EG "http://example.org/"

typedef struct {
  const SerdNode* last_base;
  const SerdNode* last_name;
  const SerdNode* last_namespace;
  const SerdNode* last_end;
  const SerdNode* last_subject;
  const SerdNode* last_predicate;
  const SerdNode* last_object;
  const SerdNode* last_graph;
  SerdStatus      return_status;
} State;

static SerdStatus
on_base(void* const handle, const SerdNode* const uri)
{
  State* const state = (State*)handle;

  state->last_base = uri;
  return state->return_status;
}

static SerdStatus
on_prefix(void* const           handle,
          const SerdNode* const name,
          const SerdNode* const uri)
{
  State* const state = (State*)handle;

  state->last_name      = name;
  state->last_namespace = uri;
  return state->return_status;
}

static SerdStatus
on_statement(void* const                   handle,
             const SerdStatementEventFlags flags,
             const SerdStatementView       statement)
{
  (void)flags;

  State* const state = (State*)handle;

  state->last_subject   = statement.subject;
  state->last_predicate = statement.predicate;
  state->last_object    = statement.object;
  state->last_graph     = statement.graph;

  return state->return_status;
}

static SerdStatus
on_end(void* const handle, const SerdNode* const node)
{
  State* const state = (State*)handle;

  state->last_end = node;
  return state->return_status;
}

static SerdStatus
on_event(void* const handle, const SerdEvent* const event)
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

  return SERD_BAD_ARG;
}

static void
test_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a sink to count the number of allocations
  SerdSink* const sink = serd_sink_new(&allocator.base, NULL, NULL, NULL);
  assert(sink);

  // Test that each allocation failing is handled gracefully
  const size_t n_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_sink_new(&allocator.base, NULL, NULL, NULL));
  }

  serd_sink_free(sink);
}

static void
test_callbacks(void)
{
  SerdNode* const base  = serd_node_new(NULL, serd_a_uri_string(NS_EG));
  SerdNode* const name  = serd_node_new(NULL, serd_a_string("eg"));
  SerdNode* const uri   = serd_node_new(NULL, serd_a_uri_string(NS_EG "uri"));
  SerdNode* const blank = serd_node_new(NULL, serd_a_blank_string("b1"));
  SerdEnv* const  env   = serd_env_new(NULL, serd_node_string_view(base));
  State           state = {0, 0, 0, 0, 0, 0, 0, 0, SERD_SUCCESS};

  const SerdStatementView statement_view = {base, uri, blank, NULL};

  const SerdBaseEvent      base_event      = {SERD_BASE, uri};
  const SerdPrefixEvent    prefix_event    = {SERD_PREFIX, name, uri};
  const SerdStatementEvent statement_event = {
    SERD_STATEMENT, 0U, statement_view};
  const SerdEndEvent end_event = {SERD_END, blank};

  // Call functions on a sink with no functions set

  SerdSink* const null_sink = serd_sink_new(NULL, &state, NULL, NULL);

  assert(!serd_sink_write_base(null_sink, base));
  assert(!serd_sink_write_prefix(null_sink, name, uri));
  assert(!serd_sink_write(null_sink, 0, base, uri, blank, NULL));
  assert(!serd_sink_write_end(null_sink, blank));

  SerdEvent event = {SERD_BASE};

  event.base = base_event;
  assert(!serd_sink_write_event(null_sink, &event));
  event.prefix = prefix_event;
  assert(!serd_sink_write_event(null_sink, &event));
  event.statement = statement_event;
  assert(!serd_sink_write_event(null_sink, &event));
  event.end = end_event;
  assert(!serd_sink_write_event(null_sink, &event));

  serd_sink_free(null_sink);

  // Try again with a sink that has the event handler set

  SerdSink* sink = serd_sink_new(NULL, &state, on_event, NULL);

  assert(!serd_sink_write_base(sink, base));
  assert(serd_node_equals(state.last_base, base));

  assert(!serd_sink_write_prefix(sink, name, uri));
  assert(serd_node_equals(state.last_name, name));
  assert(serd_node_equals(state.last_namespace, uri));

  assert(!serd_sink_write(sink, 0, base, uri, blank, NULL));
  assert(serd_node_equals(state.last_subject, base));
  assert(serd_node_equals(state.last_predicate, uri));
  assert(serd_node_equals(state.last_object, blank));
  assert(!state.last_graph);

  assert(!serd_sink_write_end(sink, blank));
  assert(serd_node_equals(state.last_end, blank));

  const SerdEvent junk = {(SerdEventType)42};
  assert(serd_sink_write_event(sink, &junk) == SERD_BAD_ARG);

  serd_sink_free(sink);
  serd_env_free(env);
  serd_node_free(NULL, blank);
  serd_node_free(NULL, uri);
  serd_node_free(NULL, name);
  serd_node_free(NULL, base);
}

static void
test_free(void)
{
  // Free of null should (as always) not crash
  serd_sink_free(NULL);

  // Set up a sink with dynamically allocated data and a free function
  uintptr_t* const data = (uintptr_t*)calloc(1, sizeof(uintptr_t));
  SerdSink* const  sink = serd_sink_new(NULL, data, NULL, free);

  // Free the sink, which should free the data (rely on valgrind or sanitizers)
  serd_sink_free(sink);
}

int
main(void)
{
  test_failed_alloc();
  test_callbacks();
  test_free();
  return 0;
}
