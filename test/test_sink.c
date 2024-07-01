// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/caret_view.h"
#include "serd/event.h"
#include "serd/field.h"
#include "serd/node.h"
#include "serd/object_view.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/token_view.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define NS_EG "http://example.org/"

typedef struct {
  ZixStringView  last_base_uri;
  ZixStringView  last_prefix_name;
  ZixStringView  last_prefix_uri;
  ZixStringView  last_end_label;
  SerdTokenView  last_subject;
  SerdTokenView  last_predicate;
  SerdObjectView last_object;
  SerdTokenView  last_graph;
  SerdStatus     return_status;
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

  state->last_subject   = statement.subject;
  state->last_predicate = statement.predicate;
  state->last_object    = statement.object;
  state->last_graph     = statement.graph;

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
  static const ZixStringView empty     = ZIX_STATIC_STRING("");
  static const ZixStringView base_str  = ZIX_STATIC_STRING(NS_EG);
  static const ZixStringView name_str  = ZIX_STATIC_STRING("eg");
  static const ZixStringView uri_str   = ZIX_STATIC_STRING(NS_EG "uri");
  static const ZixStringView blank_str = ZIX_STATIC_STRING("b1");

  static const SerdTokenView  no_tok = {empty, (SerdNodeType)0};
  static const SerdObjectView no_obj = {empty, (SerdNodeType)0, 0U, no_tok};

  static const SerdTokenView  base_tok  = {base_str, SERD_URI};
  static const SerdTokenView  uri_tok   = {uri_str, SERD_URI};
  static const SerdObjectView blank_obj = {blank_str, SERD_BLANK, 0U, no_tok};

  State state = {
    empty, empty, empty, empty, no_tok, no_tok, no_obj, no_tok, SERD_SUCCESS};

  const SerdStatementView statement_view =
    serd_statement_view(base_tok, uri_tok, blank_obj, no_tok);

  const SerdCaretView caret_view = {base_tok, 1, 1};

  // Call functions on a sink with no functions set

  SerdSink* const null_sink = serd_sink_new(NULL, &state, NULL, NULL);

  assert(!serd_sink_write_event(null_sink, serd_base_event(base_str)));
  assert(
    !serd_sink_write_event(null_sink, serd_prefix_event(name_str, uri_str)));
  assert(!serd_sink_write_event(
    null_sink, serd_statement_event(0U, statement_view, caret_view)));
  assert(!serd_sink_write_event(null_sink, serd_end_event(blank_str)));

  assert(!serd_sink_write_event(null_sink, serd_base_event(uri_str)));
  assert(
    !serd_sink_write_event(null_sink, serd_prefix_event(name_str, uri_str)));
  assert(!serd_sink_write_event(
    null_sink, serd_statement_event(0U, statement_view, caret_view)));
  assert(!serd_sink_write_event(null_sink, serd_end_event(blank_str)));

  serd_sink_free(null_sink);

  // Try again with a sink that has the event handler set

  SerdSink* sink = serd_sink_new(NULL, &state, on_event, NULL);

  assert(!serd_sink_write_event(sink, serd_base_event(base_str)));
  assert(zix_string_view_equals(state.last_base_uri, base_str));

  assert(!serd_sink_write_event(sink, serd_prefix_event(name_str, uri_str)));
  assert(zix_string_view_equals(state.last_prefix_name, name_str));
  assert(zix_string_view_equals(state.last_prefix_uri, uri_str));

  assert(!serd_sink_write_event(
    sink,
    serd_statement_event(
      0U,
      serd_statement_view(base_tok, uri_tok, blank_obj, no_tok),
      caret_view)));
  assert(token_equals(base_tok, state.last_subject));
  assert(token_equals(uri_tok, state.last_predicate));
  assert(object_equals(blank_obj, state.last_object));
  assert(!serd_field_supports(SERD_GRAPH, state.last_graph.type));

  assert(!serd_sink_write_event(sink, serd_end_event(blank_str)));
  assert(zix_string_view_equals(state.last_end_label, blank_str));

  const SerdEvent junk = {(SerdEventType)42};
  assert(serd_sink_write_event(sink, junk) == SERD_BAD_ARG);

  serd_sink_free(sink);
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
