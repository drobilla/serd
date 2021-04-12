/*
  Copyright 2019-2020 David Robillard <d@drobilla.net>

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

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>

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
on_statement(void*                handle,
             SerdStatementFlags   flags,
             const SerdStatement* statement)
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

static SerdStatus
on_event(void* handle, const SerdEvent* event)
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

  return SERD_SUCCESS;
}

int
main(void)
{
  SerdNodes* const nodes = serd_nodes_new();

  const SerdNode* base =
    serd_nodes_manage(nodes, serd_new_uri(SERD_STATIC_STRING(NS_EG)));

  const SerdNode* name =
    serd_nodes_manage(nodes, serd_new_string(SERD_STATIC_STRING("eg")));

  const SerdNode* uri =
    serd_nodes_manage(nodes, serd_new_uri(SERD_STATIC_STRING(NS_EG "uri")));

  const SerdNode* blank =
    serd_nodes_manage(nodes, serd_new_blank(SERD_STATIC_STRING("b1")));

  SerdEnv* env = serd_env_new(serd_node_string_view(base));

  SerdStatement* const statement =
    serd_statement_new(base, uri, blank, NULL, NULL);

  State state = {0, 0, 0, 0, 0, SERD_SUCCESS};

  // Call functions on a sink with no functions set

  SerdSink* null_sink = serd_sink_new(&state, NULL, NULL);
  assert(!serd_sink_write_base(null_sink, base));
  assert(!serd_sink_write_prefix(null_sink, name, uri));
  assert(!serd_sink_write_statement(null_sink, 0, statement));
  assert(!serd_sink_write(null_sink, 0, base, uri, blank, NULL));
  assert(!serd_sink_write_end(null_sink, blank));
  serd_sink_free(null_sink);

  // Try again with a sink that has the event handler set

  SerdSink* sink = serd_sink_new(&state, on_event, NULL);

  assert(!serd_sink_write_base(sink, base));
  assert(serd_node_equals(state.last_base, base));

  assert(!serd_sink_write_prefix(sink, name, uri));
  assert(serd_node_equals(state.last_name, name));
  assert(serd_node_equals(state.last_namespace, uri));

  assert(!serd_sink_write_statement(sink, 0, statement));
  assert(serd_statement_equals(state.last_statement, statement));

  assert(!serd_sink_write_end(sink, blank));
  assert(serd_node_equals(state.last_end, blank));

  const SerdEvent corrupt = {(SerdEventType)42};
  assert(serd_sink_write_event(sink, &corrupt) == SERD_ERR_BAD_ARG);

  serd_sink_free(sink);
  serd_statement_free(statement);
  serd_env_free(env);
  serd_nodes_free(nodes);

  return 0;
}
