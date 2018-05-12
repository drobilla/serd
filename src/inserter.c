/*
  Copyright 2011-2018 David Robillard <http://drobilla.net>

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

#include "model.h"
#include "sink.h"
#include "statement.h"
#include "world.h"

#include "serd/serd.h"

#include <stdlib.h>

struct SerdInserterImpl {
	SerdModel* model;
	SerdNode*  default_graph;
	SerdSink   iface;
};

static const SerdNode*
manage_or_intern(SerdNodes* nodes, SerdNode* manage, const SerdNode* intern)
{
	return manage ? serd_nodes_manage(nodes, manage)
	              : serd_nodes_intern(nodes, intern);
}

static SerdStatus
serd_inserter_on_statement(SerdInserter*            inserter,
                           const SerdStatementFlags flags,
                           const SerdStatement*     statement)
{
	(void)flags;

	const SerdNode* const subject   = serd_statement_get_subject(statement);
	const SerdNode* const predicate = serd_statement_get_predicate(statement);
	const SerdNode* const object    = serd_statement_get_object(statement);
	const SerdNode* const graph     = serd_statement_get_graph(statement);

	// Attempt to expand all nodes to eliminate CURIEs
	SerdNode* const s = serd_env_expand(inserter->iface.env, subject);
	SerdNode* const p = serd_env_expand(inserter->iface.env, predicate);
	SerdNode* const o = serd_env_expand(inserter->iface.env, object);
	SerdNode* const g = serd_env_expand(inserter->iface.env, graph);

	SerdNodes* const nodes       = inserter->model->world->nodes;
	const SerdNode*  model_graph = manage_or_intern(nodes, g, graph);
	if (!model_graph) {
		model_graph = serd_nodes_intern(nodes, inserter->default_graph);
	}

	const SerdStatus st = serd_model_add_internal(
		inserter->model,
		(inserter->model->flags & SERD_STORE_CURSORS) ? statement->cursor
		: NULL,
		manage_or_intern(nodes, s, subject),
		manage_or_intern(nodes, p, predicate),
		manage_or_intern(nodes, o, object),
		model_graph);

	return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

SerdInserter*
serd_inserter_new(SerdModel* model, SerdEnv* env, const SerdNode* default_graph)
{
	SerdInserter* inserter    = (SerdInserter*)calloc(1, sizeof(SerdInserter));
	inserter->model           = model;
	inserter->default_graph   = serd_node_copy(default_graph);
	inserter->iface.handle    = inserter;
	inserter->iface.env       = env;
	inserter->iface.statement = (SerdStatementFunc)serd_inserter_on_statement;
	return inserter;
}

void
serd_inserter_free(SerdInserter* inserter)
{
	if (inserter) {
		serd_node_free(inserter->default_graph);
		free(inserter);
	}
}

const SerdSink*
serd_inserter_get_sink(SerdInserter* inserter)
{
	return &inserter->iface;
}
