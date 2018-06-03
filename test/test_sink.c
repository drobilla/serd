// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/caret_view.h"
#include "serd/event.h"
#include "serd/field.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/object_view.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/token_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define NS_EG "http://example.org/"

typedef struct {
  ZixStringView     last_base_uri;
  ZixStringView     last_prefix_name;
  ZixStringView     last_prefix_uri;
  ZixStringView     last_end_label;
  SerdStatementView last_statement;
  SerdStatus        return_status;
} State;

static bool
token_equals(const SerdTokenView lhs, const SerdTokenView rhs)
{
  return lhs.type == rhs.type && zix_string_view_equals(lhs.string, rhs.string);
}

static bool
object_equals(const SerdObjectView lhs, const SerdObjectView rhs)
{
  return lhs.type == rhs.type && lhs.flags == rhs.flags &&
         zix_string_view_equals(lhs.string, rhs.string) &&
         token_equals(lhs.meta, rhs.meta);
}

static SerdStatus
on_base(void* const handle, const ZixStringView uri)
{
  State* const state = (State*)handle;

  state->last_base_uri = uri;
  return state->return_status;
}

static SerdStatus
on_prefix(void* const handle, const ZixStringView name, const ZixStringView uri)
{
  State* const state = (State*)handle;

  state->last_prefix_name = name;
  state->last_prefix_uri  = uri;
  return state->return_status;
}

static SerdStatus
on_statement(void* const                   handle,
             const SerdStatementEventFlags flags,
             const SerdStatementView       statement)
{
  (void)flags;

  State* const state = (State*)handle;

  state->last_statement = statement;

  return state->return_status;
}

static SerdStatus
on_end(void* const handle, const ZixStringView label)
{
  State* const state = (State*)handle;

  state->last_end_label = label;
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
    return on_end(handle, event->end.label);
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
  serd_sink_free(sink);

  // Test that each allocation failing is handled gracefully
  const size_t n_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_sink_new(&allocator.base, NULL, NULL, NULL));
  }
}

static void
test_callbacks(void)
{
  static const SerdCaretView  no_caret  = {NULL, 0U, 0U};
  static const SerdTokenView  no_token  = {(SerdNodeType)0, {"", 0U}};
  static const SerdObjectView no_object = {
    (SerdNodeType)0, 0U, {"", 0U}, no_token};

  static const SerdTokenView  no_token  = {(SerdNodeType)0, empty};
  static const SerdObjectView no_object = {
    (SerdNodeType)0, 0U, empty, no_token};

  State state = {NULL,
                 NULL,
                 NULL,
                 NULL,
                 {no_token, no_token, no_object, no_token, no_caret},
                 SERD_SUCCESS};

  const SerdStatementView statement_view = {serd_node_token_view(base),
                                            serd_node_token_view(uri),
                                            serd_node_object_view(blank),
                                            no_token,
                                            {NULL, 0, 0}};

  const SerdStatementView statement_view = {
    {SERD_URI, base_str},
    {SERD_URI, uri_str},
    {SERD_BLANK, 0U, blank_str, no_token},
    no_token,
  };

  const SerdBaseEvent      base_event      = {SERD_BASE, uri_str};
  const SerdPrefixEvent    prefix_event    = {SERD_PREFIX, name_str, uri_str};
  const SerdStatementEvent statement_event = {
    SERD_STATEMENT, 0U, statement_view, {base, 1, 1}};
  const SerdEndEvent end_event = {SERD_END, blank_str};

  // Call functions on a sink with no functions set

  SerdSink* const null_sink = serd_sink_new(NULL, &state, NULL, NULL);

  assert(!serd_sink_write_base(null_sink, base));
  assert(!serd_sink_write_prefix(null_sink, name, uri));
  assert(!serd_sink_write_statement(null_sink, 0, statement_view));
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

  assert(!serd_sink_write_base(sink, base_str));
  assert(zix_string_view_equals(state.last_base_uri, base_str));

  assert(!serd_sink_write_prefix(sink, name_str, uri_str));
  assert(zix_string_view_equals(state.last_prefix_name, name_str));
  assert(zix_string_view_equals(state.last_prefix_uri, uri_str));

  assert(!serd_sink_write_statement(sink, 0, statement_view));
  assert(serd_node_equals_token_view(base, state.last_statement.subject));
  assert(serd_node_equals_token_view(uri, state.last_statement.predicate));
  assert(serd_node_equals_object_view(blank, state.last_statement.object));
  assert(!serd_field_supports(SERD_GRAPH, state.last_statement.graph.type));

  assert(!serd_sink_write_end(sink, blank_str));
  assert(zix_string_view_equals(state.last_end_label, blank_str));

  const SerdEvent junk = {(SerdEventType)42};
  assert(serd_sink_write_event(sink, &junk) == SERD_BAD_ARG);

  serd_sink_free(sink);
  serd_nodes_free(nodes);
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
