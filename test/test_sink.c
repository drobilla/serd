// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define NS_EG "http://example.org/"

typedef struct {
  const SerdNode*      last_base;
  const SerdNode*      last_name;
  const SerdNode*      last_namespace;
  const SerdNode*      last_end;
  const SerdStatement* last_statement;
  SerdStatus           return_status;
} State;

static SerdStatus
on_base(void* handle, const SerdNode* uri)
{
  State* state = (State*)handle;

  state->last_base = uri;
  return state->return_status;
}

static SerdStatus
on_prefix(void* handle, const SerdNode* name, const SerdNode* uri)
{
  State* state = (State*)handle;

  state->last_name      = name;
  state->last_namespace = uri;
  return state->return_status;
}

static SerdStatus
on_statement(void*                      handle,
             SerdStatementFlags         flags,
             const SerdStatement* const statement)
{
  (void)flags;

  State* state = (State*)handle;

  state->last_statement = statement;

  return state->return_status;
}

static SerdStatus
on_end(void* handle, const SerdNode* node)
{
  State* state = (State*)handle;

  state->last_end = node;
  return state->return_status;
}

static void
test_callbacks(void)
{
  SerdNode* const base  = serd_new_uri(serd_string(NS_EG));
  SerdNode* const name  = serd_new_string(serd_string("eg"));
  SerdNode* const uri   = serd_new_uri(serd_string(NS_EG "uri"));
  SerdNode* const blank = serd_new_blank(serd_string("b1"));
  SerdEnv*        env   = serd_env_new(serd_node_string_view(base));
  State           state = {0, 0, 0, 0, 0, SERD_SUCCESS};

  SerdStatement* const statement =
    serd_statement_new(base, uri, blank, NULL, NULL);

  // Call functions on a sink with no functions set

  SerdSink* null_sink = serd_sink_new(&state, NULL);
  assert(!serd_sink_write_base(null_sink, base));
  assert(!serd_sink_write_prefix(null_sink, name, uri));
  assert(!serd_sink_write_statement(null_sink, 0, statement));
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

  assert(!serd_sink_write_statement(sink, 0, statement));
  assert(serd_statement_equals(state.last_statement, statement));

  assert(!serd_sink_write_end(sink, blank));
  assert(serd_node_equals(state.last_end, blank));

  serd_sink_free(sink);

  serd_statement_free(statement);
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
  uintptr_t* data = (uintptr_t*)calloc(1, sizeof(uintptr_t));
  SerdSink*  sink = serd_sink_new(data, free);

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
