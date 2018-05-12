/*
  Copyright 2011-2019 David Robillard <http://drobilla.net>

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

int
main(void)
{
	SerdNodes* const nodes = serd_nodes_new();

	const SerdNode* const f = serd_nodes_manage(nodes, serd_new_string("file"));
	const SerdNode* const s = serd_nodes_manage(nodes, serd_new_uri(NS_EG "s"));
	const SerdNode* const p = serd_nodes_manage(nodes, serd_new_uri(NS_EG "p"));
	const SerdNode* const o = serd_nodes_manage(nodes, serd_new_uri(NS_EG "o"));
	const SerdNode* const g = serd_nodes_manage(nodes, serd_new_uri(NS_EG "g"));

	assert(!serd_statement_copy(NULL));

	SerdCursor* const    cursor    = serd_cursor_new(f, 1, 1);
	SerdStatement* const statement = serd_statement_new(s, p, o, g, cursor);
	assert(serd_statement_equals(statement, statement));
	assert(!serd_statement_equals(statement, NULL));
	assert(!serd_statement_equals(NULL, statement));
	assert(serd_statement_get_subject(statement) == s);
	assert(serd_statement_get_predicate(statement) == p);
	assert(serd_statement_get_object(statement) == o);
	assert(serd_statement_get_graph(statement) == g);
	assert(serd_statement_get_cursor(statement) != cursor);
	assert(serd_cursor_equals(serd_statement_get_cursor(statement), cursor));
	assert(serd_statement_matches(statement, s, p, o, g));
	assert(serd_statement_matches(statement, NULL, p, o, g));
	assert(serd_statement_matches(statement, s, NULL, o, g));
	assert(serd_statement_matches(statement, s, p, NULL, g));
	assert(serd_statement_matches(statement, s, p, o, NULL));
	assert(!serd_statement_matches(statement, o, NULL, NULL, NULL));
	assert(!serd_statement_matches(statement, NULL, o, NULL, NULL));
	assert(!serd_statement_matches(statement, NULL, NULL, s, NULL));
	assert(!serd_statement_matches(statement, NULL, NULL, NULL, s));

	SerdStatement* const diff_s = serd_statement_new(o, p, o, g, cursor);
	assert(!serd_statement_equals(statement, diff_s));
	serd_statement_free(diff_s);

	SerdStatement* const diff_p = serd_statement_new(s, o, o, g, cursor);
	assert(!serd_statement_equals(statement, diff_p));
	serd_statement_free(diff_p);

	SerdStatement* const diff_o = serd_statement_new(s, p, s, g, cursor);
	assert(!serd_statement_equals(statement, diff_o));
	serd_statement_free(diff_o);

	SerdStatement* const diff_g = serd_statement_new(s, p, o, s, cursor);
	assert(!serd_statement_equals(statement, diff_g));
	serd_statement_free(diff_g);

	serd_statement_free(statement);
	serd_cursor_free(cursor);
	serd_nodes_free(nodes);

	return 0;
}
