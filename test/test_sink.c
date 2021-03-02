// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "zix/string_view.h"

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
on_statement(void* const              handle,
             const SerdStatementFlags flags,
             const SerdStatementView  statement)
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

static void
test_callbacks(void)
{
  SerdNode* const base  = serd_new_uri(zix_string(NS_EG));
  SerdNode* const name  = serd_new_string(zix_string("eg"));
  SerdNode* const uri   = serd_new_uri(zix_string(NS_EG "uri"));
  SerdNode* const blank = serd_new_blank(zix_string("b1"));
  SerdEnv* const  env   = serd_env_new(serd_node_string_view(base));
  State           state = {0, 0, 0, 0, 0, 0, 0, 0, SERD_SUCCESS};

  // Call functions on a sink with no functions set

  SerdSink* const null_sink = serd_sink_new(&state, NULL);
  assert(!serd_sink_write_base(null_sink, base));
  assert(!serd_sink_write_prefix(null_sink, name, uri));
  assert(!serd_sink_write(null_sink, 0, base, uri, blank, NULL));
  assert(!serd_sink_write_end(null_sink, blank));
  serd_sink_free(null_sink);

  // Try again with a sink that has the event handler set

  SerdSink* sink = serd_sink_new(&state, NULL);
  serd_sink_set_base_func(sink, on_base);
  serd_sink_set_prefix_func(sink, on_prefix);
  serd_sink_set_statement_func(sink, on_statement);
  serd_sink_set_end_func(sink, on_end);

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

  serd_sink_free(sink);
  serd_env_free(env);
  serd_node_free(blank);
  serd_node_free(uri);
  serd_node_free(name);
  serd_node_free(base);
}

static void
test_free(void)
{
  // Free of null should (as always) not crash
  serd_sink_free(NULL);

  // Set up a sink with dynamically allocated data and a free function
  uintptr_t* const data = (uintptr_t*)calloc(1, sizeof(uintptr_t));
  SerdSink* const  sink = serd_sink_new(data, free);

  // Free the sink, which should free the data (rely on valgrind or sanitizers)
  serd_sink_free(sink);
}

int
main(void)
{
  test_callbacks();
  test_free();
  return 0;
}
