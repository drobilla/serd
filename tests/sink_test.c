/*
  Copyright 2019 David Robillard <http://drobilla.net>

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

typedef struct
{
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

int
main(void)
{
	SerdNodes* const nodes = serd_nodes_new();

	const SerdNode* base  = serd_nodes_manage(nodes, serd_new_uri(NS_EG));
	const SerdNode* name  = serd_nodes_manage(nodes, serd_new_string("eg"));
	const SerdNode* uri   = serd_nodes_manage(nodes, serd_new_uri(NS_EG "uri"));
	const SerdNode* blank = serd_nodes_manage(nodes, serd_new_blank("b1"));
	SerdEnv*        env   = serd_env_new(base);

	SerdStatement* const statement =
		serd_statement_new(base, uri, blank, NULL, NULL);

	State     state = {0, 0, 0, 0, 0, SERD_SUCCESS};
	SerdSink* sink  = serd_sink_new(&state, env);

	assert(serd_sink_get_env(sink) == env);

	// Call without having any functions set

	assert(!serd_sink_write_base(sink, base));
	assert(!serd_sink_write_prefix(sink, name, uri));
	assert(!serd_sink_write_statement(sink, 0, statement));
	assert(!serd_sink_write(sink, 0, base, uri, blank, NULL));
	assert(!serd_sink_write_end(sink, blank));

	// Set functions and try again

	serd_sink_set_base_func(sink, on_base);
	assert(!serd_sink_write_base(sink, base));
	assert(serd_node_equals(state.last_base, base));

	serd_sink_set_prefix_func(sink, on_prefix);
	assert(!serd_sink_write_prefix(sink, name, uri));
	assert(serd_node_equals(state.last_name, name));
	assert(serd_node_equals(state.last_namespace, uri));

	serd_sink_set_statement_func(sink, on_statement);
	assert(!serd_sink_write_statement(sink, 0, statement));
	assert(serd_statement_equals(state.last_statement, statement));

	serd_sink_set_end_func(sink, on_end);
	assert(!serd_sink_write_end(sink, blank));
	assert(serd_node_equals(state.last_end, blank));

	serd_sink_free(sink);
	serd_statement_free(statement);
	serd_env_free(env);
	serd_nodes_free(nodes);

	return 0;
}
