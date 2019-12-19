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

#include "namespaces.h"
#include "node.h"
#include "sink.h"
#include "statement.h"
#include "string_utils.h"

#include "serd/serd.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
	const SerdSink* target;
	SerdNode*       subject;
	SerdNode*       predicate;
	SerdNode*       object;
	SerdNode*       graph;
} SerdFilterData;

struct SerdFilterImpl
{
	SerdSink       sink;
	SerdFilterData data;
};

static SerdStatus
serd_filter_on_statement(void*                handle,
                         SerdStatementFlags   flags,
                         const SerdStatement* statement)
{
	SerdFilterData* data = (SerdFilterData*)handle;
	(void)handle;
	(void)flags;
	(void)statement;

	if (serd_statement_matches(statement,
	                           data->subject,
	                           data->predicate,
	                           data->object,
	                           data->graph)) {
		serd_sink_write_statement(data->target, flags, statement);
	}

	return SERD_SUCCESS;
}

SerdFilter*
serd_filter_new(const SerdSink* target)
{
	SerdFilter*     filter = (SerdFilter*)calloc(1, sizeof(SerdFilter));
	SerdSink*       sink   = &filter->sink;
	SerdFilterData* data   = &filter->data;

	sink->handle      = data;
	sink->free_handle = NULL;
	sink->env         = target->env;
	data->target      = target;

	serd_sink_set_statement_func(sink, serd_filter_on_statement);

	return filter;
}

void
serd_filter_free(SerdFilter* filter)
{
	if (filter) {
		serd_node_free(filter->data.subject);
		serd_node_free(filter->data.predicate);
		serd_node_free(filter->data.object);
		serd_node_free(filter->data.graph);
		free(filter);
	}
}

static void
set_field(SerdNode** field, const SerdNode* pattern)
{
	const bool is_var =
	        (!pattern || serd_node_get_type(pattern) == SERD_VARIABLE);

	serd_node_free(*field);
	*field = (is_var ? NULL : serd_node_copy(pattern));
}

SerdStatus
serd_filter_set_statement(SerdFilter*     filter,
                          const SerdNode* subject,
                          const SerdNode* predicate,
                          const SerdNode* object,
                          const SerdNode* graph)
{
	set_field(&filter->data.subject, subject);
	set_field(&filter->data.predicate, predicate);
	set_field(&filter->data.object, object);
	set_field(&filter->data.graph, graph);

	return SERD_SUCCESS;
}

const SerdSink*
serd_filter_get_sink(const SerdFilter* filter)
{
	return &filter->sink;
}
