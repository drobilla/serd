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

#include "statement.h"

#include "cursor.h"
#include "node.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

SerdStatement*
serd_statement_new(const SerdNode*   s,
                   const SerdNode*   p,
                   const SerdNode*   o,
                   const SerdNode*   g,
                   const SerdCursor* cursor)
{
	SerdStatement* statement = (SerdStatement*)malloc(sizeof(SerdStatement));
	if (statement) {
		statement->nodes[0] = s;
		statement->nodes[1] = p;
		statement->nodes[2] = o;
		statement->nodes[3] = g;
		statement->cursor   = serd_cursor_copy(cursor);
	}
	return statement;
}

SerdStatement*
serd_statement_copy(const SerdStatement* statement)
{
	if (!statement) {
		return NULL;
	}

	SerdStatement* copy = (SerdStatement*)malloc(sizeof(SerdStatement));
	memcpy(copy, statement, sizeof(SerdStatement));
	if (statement->cursor) {
		copy->cursor = (SerdCursor*)malloc(sizeof(SerdCursor));
		memcpy(copy->cursor, statement->cursor, sizeof(SerdCursor));
	}
	return copy;
}

void
serd_statement_free(SerdStatement* statement)
{
	free(statement->cursor);
	free(statement);
}

const SerdNode*
serd_statement_get_node(const SerdStatement* statement, SerdField field)
{
	return statement->nodes[field];
}

const SerdNode*
serd_statement_get_subject(const SerdStatement* statement)
{
	return statement->nodes[SERD_SUBJECT];
}

const SerdNode*
serd_statement_get_predicate(const SerdStatement* statement)
{
	return statement->nodes[SERD_PREDICATE];
}

const SerdNode*
serd_statement_get_object(const SerdStatement* statement)
{
	return statement->nodes[SERD_OBJECT];
}

const SerdNode*
serd_statement_get_graph(const SerdStatement* statement)
{
	return statement->nodes[SERD_GRAPH];
}

const SerdCursor*
serd_statement_get_cursor(const SerdStatement* statement)
{
	return statement->cursor;
}

bool
serd_statement_equals(const SerdStatement* a, const SerdStatement* b)
{
	return (a == b || (a && b && serd_node_equals(a->nodes[0], b->nodes[0]) &&
	                   serd_node_equals(a->nodes[1], b->nodes[1]) &&
	                   serd_node_equals(a->nodes[2], b->nodes[2]) &&
	                   serd_node_equals(a->nodes[3], b->nodes[3])));
}

bool
serd_statement_matches(const SerdStatement* statement,
                       const SerdNode*      subject,
                       const SerdNode*      predicate,
                       const SerdNode*      object,
                       const SerdNode*      graph)
{
	return (serd_node_pattern_match(statement->nodes[0], subject) &&
	        serd_node_pattern_match(statement->nodes[1], predicate) &&
	        serd_node_pattern_match(statement->nodes[2], object) &&
	        serd_node_pattern_match(statement->nodes[3], graph));
}

bool
serd_statement_matches_quad(const SerdStatement* statement, const SerdQuad quad)
{
	return serd_statement_matches(
		statement, quad[0], quad[1], quad[2], quad[3]);
}
