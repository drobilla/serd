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

#undef NDEBUG

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"

#define WILDCARD_NODE NULL

#define NS_RDF    "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define RDF_FIRST NS_RDF "first"
#define RDF_REST  NS_RDF "rest"

#define N_OBJECTS_PER 2U

typedef const SerdNode* Quad[4];

typedef struct
{
	Quad query;
	int  expected_num_results;
} QueryTest;

static const SerdNode*
manage(SerdWorld* world, SerdNode* node)
{
	return serd_nodes_manage(serd_world_get_nodes(world), node);
}

static const SerdNode*
uri(SerdWorld* world, const size_t num)
{
	char str[] = "eg:000";
	snprintf(str + 3, 4, "%03zu", num);
	return manage(world, serd_new_uri(str));
}

static int
generate(SerdWorld*      world,
         SerdModel*      model,
         size_t          n_quads,
         const SerdNode* graph)
{
	SerdNodes* nodes = serd_world_get_nodes(world);

	for (size_t i = 0; i < n_quads; ++i) {
		size_t num = (i * N_OBJECTS_PER) + 1U;

		const SerdNode* ids[2 + N_OBJECTS_PER];
		for (size_t j = 0; j < 2 + N_OBJECTS_PER; ++j) {
			ids[j] = uri(world, num++);
		}

		for (size_t j = 0; j < N_OBJECTS_PER; ++j) {
			assert(!serd_model_add(model, ids[0], ids[1], ids[2 + j], graph));
		}
	}

	// Add some literals

	// (98 4 "hello") and (98 4 "hello"^^<5>)
	const SerdNode* hello = manage(world, serd_new_string("hello"));
	const SerdNode* hello_gb =
	        manage(world, serd_new_plain_literal("hello", "en-gb"));
	const SerdNode* hello_us =
	        manage(world, serd_new_plain_literal("hello", "en-us"));
	const SerdNode* hello_t4 = serd_nodes_manage(
	        nodes, serd_new_typed_literal("hello", uri(world, 4)));
	const SerdNode* hello_t5 = serd_nodes_manage(
	        nodes, serd_new_typed_literal("hello", uri(world, 5)));
	assert(!serd_model_add(model, uri(world, 98), uri(world, 4), hello, graph));
	assert(!serd_model_add(
	        model, uri(world, 98), uri(world, 4), hello_t5, graph));

	// (96 4 "hello"^^<4>) and (96 4 "hello"^^<5>)
	assert(!serd_model_add(
	        model, uri(world, 96), uri(world, 4), hello_t4, graph));
	assert(!serd_model_add(
	        model, uri(world, 96), uri(world, 4), hello_t5, graph));

	// (94 5 "hello") and (94 5 "hello"@en-gb)
	assert(!serd_model_add(model, uri(world, 94), uri(world, 5), hello, graph));
	assert(!serd_model_add(
	        model, uri(world, 94), uri(world, 5), hello_gb, graph));

	// (92 6 "hello"@en-us) and (92 6 "hello"@en-gb)
	assert(!serd_model_add(
	        model, uri(world, 92), uri(world, 6), hello_us, graph));
	assert(!serd_model_add(
	        model, uri(world, 92), uri(world, 6), hello_gb, graph));

	// (14 6 "bonjour"@fr) and (14 6 "salut"@fr)
	const SerdNode* bonjour =
	        manage(world, serd_new_plain_literal("bonjour", "fr"));
	const SerdNode* salut =
	        manage(world, serd_new_plain_literal("salut", "fr"));
	assert(!serd_model_add(
	        model, uri(world, 14), uri(world, 6), bonjour, graph));
	assert(!serd_model_add(model, uri(world, 14), uri(world, 6), salut, graph));

	// Attempt to add duplicates
	assert(serd_model_add(model, uri(world, 14), uri(world, 6), salut, graph));

	// Add a blank node subject
	const SerdNode* ablank = manage(world, serd_new_blank("ablank"));
	assert(!serd_model_add(model, ablank, uri(world, 6), salut, graph));

	// Add statement with URI object
	assert(!serd_model_add(model, ablank, uri(world, 6), uri(world, 7), graph));

	return EXIT_SUCCESS;
}

static int
test_read(SerdWorld*      world,
          SerdModel*      model,
          const SerdNode* g,
          const size_t    n_quads)
{
	SerdIter*            iter = serd_model_begin(model);
	const SerdStatement* prev = NULL;
	for (; !serd_iter_equals(iter, serd_model_end(model));
	     serd_iter_next(iter)) {
		const SerdStatement* statement = serd_iter_get(iter);
		assert(serd_statement_get_subject(statement));
		assert(serd_statement_get_predicate(statement));
		assert(serd_statement_get_object(statement));
		assert(!serd_statement_equals(statement, prev));
		assert(!serd_statement_equals(prev, statement));
		prev = statement;
	}

	// Attempt to increment past end
	assert(serd_iter_next(iter));
	serd_iter_free(iter);

	const char*     s           = "hello";
	const SerdNode* plain_hello = manage(world, serd_new_string(s));
	const SerdNode* type4_hello =
	        manage(world, serd_new_typed_literal(s, uri(world, 4)));
	const SerdNode* type5_hello =
	        manage(world, serd_new_typed_literal(s, uri(world, 5)));
	const SerdNode* gb_hello =
	        manage(world, serd_new_plain_literal(s, "en-gb"));
	const SerdNode* us_hello =
	        manage(world, serd_new_plain_literal(s, "en-us"));

#define NUM_PATTERNS 18

	QueryTest patterns[NUM_PATTERNS] = {
	        {{NULL, NULL, NULL}, (int)(n_quads * N_OBJECTS_PER) + 12},
	        {{uri(world, 1), WILDCARD_NODE, WILDCARD_NODE}, 2},
	        {{uri(world, 9), uri(world, 9), uri(world, 9)}, 0},
	        {{uri(world, 1), uri(world, 2), uri(world, 4)}, 1},
	        {{uri(world, 3), uri(world, 4), WILDCARD_NODE}, 2},
	        {{WILDCARD_NODE, uri(world, 2), uri(world, 4)}, 1},
	        {{WILDCARD_NODE, WILDCARD_NODE, uri(world, 4)}, 1},
	        {{uri(world, 1), WILDCARD_NODE, WILDCARD_NODE}, 2},
	        {{uri(world, 1), WILDCARD_NODE, uri(world, 4)}, 1},
	        {{WILDCARD_NODE, uri(world, 2), WILDCARD_NODE}, 2},
	        {{uri(world, 98), uri(world, 4), plain_hello}, 1},
	        {{uri(world, 98), uri(world, 4), type5_hello}, 1},
	        {{uri(world, 96), uri(world, 4), type4_hello}, 1},
	        {{uri(world, 96), uri(world, 4), type5_hello}, 1},
	        {{uri(world, 94), uri(world, 5), plain_hello}, 1},
	        {{uri(world, 94), uri(world, 5), gb_hello}, 1},
	        {{uri(world, 92), uri(world, 6), gb_hello}, 1},
	        {{uri(world, 92), uri(world, 6), us_hello}, 1}};

	Quad match = {uri(world, 1), uri(world, 2), uri(world, 4), g};
	assert(serd_model_ask(model, match[0], match[1], match[2], match[3]));

	Quad nomatch = {uri(world, 1), uri(world, 2), uri(world, 9), g};
	assert(!serd_model_ask(
	        model, nomatch[0], nomatch[1], nomatch[2], nomatch[3]));

	assert(!serd_model_get(model, NULL, NULL, uri(world, 3), g));
	assert(!serd_model_get(model, uri(world, 1), uri(world, 99), NULL, g));

	assert(serd_node_equals(
	        serd_model_get(model, uri(world, 1), uri(world, 2), NULL, g),
	        uri(world, 3)));
	assert(serd_node_equals(
	        serd_model_get(model, uri(world, 1), NULL, uri(world, 3), g),
	        uri(world, 2)));
	assert(serd_node_equals(
	        serd_model_get(model, NULL, uri(world, 2), uri(world, 3), g),
	        uri(world, 1)));

	for (unsigned i = 0; i < NUM_PATTERNS; ++i) {
		QueryTest test = patterns[i];
		Quad      pat  = {test.query[0], test.query[1], test.query[2], g};

		SerdRange* range =
		        serd_model_range(model, pat[0], pat[1], pat[2], pat[3]);
		int num_results = 0;
		for (; !serd_range_empty(range); serd_range_next(range)) {
			++num_results;
			assert(serd_statement_matches(
			        serd_range_front(range), pat[0], pat[1], pat[2], pat[3]));
		}
		serd_range_free(range);

		assert(num_results == test.expected_num_results);
	}

	// Query blank node subject
	const SerdNode* ablank      = manage(world, serd_new_blank("ablank"));
	Quad            pat         = {ablank, 0, 0};
	int             num_results = 0;
	SerdRange* range = serd_model_range(model, pat[0], pat[1], pat[2], pat[3]);
	for (; !serd_range_empty(range); serd_range_next(range)) {
		++num_results;
		const SerdStatement* statement = serd_range_front(range);
		assert(serd_statement_matches(
		        statement, pat[0], pat[1], pat[2], pat[3]));
	}
	serd_range_free(range);

	assert(num_results == 2);

	// Test nested queries
	const SerdNode* last_subject = 0;
	range = serd_model_range(model, NULL, NULL, NULL, NULL);
	for (; !serd_range_empty(range); serd_range_next(range)) {
		const SerdStatement* statement = serd_range_front(range);
		const SerdNode*      subject   = serd_statement_get_subject(statement);
		if (subject == last_subject) {
			continue;
		}

		Quad       subpat   = {subject, 0, 0};
		SerdRange* subrange = serd_model_range(
		        model, subpat[0], subpat[1], subpat[2], subpat[3]);
		const SerdStatement* substatement    = serd_range_front(subrange);
		uint64_t             num_sub_results = 0;
		assert(serd_statement_get_subject(substatement) == subject);
		for (; !serd_range_empty(subrange); serd_range_next(subrange)) {
			assert(serd_statement_matches(serd_range_front(subrange),
			                              subpat[0],
			                              subpat[1],
			                              subpat[2],
			                              subpat[3]));
			++num_sub_results;
		}
		serd_range_free(subrange);
		assert(num_sub_results == N_OBJECTS_PER);

		uint64_t count = serd_model_count(model, subject, 0, 0, 0);
		assert(count == num_sub_results);

		last_subject = subject;
	}
	serd_range_free(range);

	return 0;
}

static SerdStatus
expected_error(void* handle, const SerdLogEntry* entry)
{
	(void)handle;

	fprintf(stderr, "expected: ");
	vfprintf(stderr, entry->fmt, *entry->args);
	return SERD_SUCCESS;
}

static int
test_free_null(SerdWorld* world, const size_t n_quads)
{
	(void)world;
	(void)n_quads;

	serd_model_free(NULL); // Shouldn't crash
	return 0;
}

static int
test_get_world(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);
	assert(serd_model_get_world(model) == world);
	serd_model_free(model);
	return 0;
}

static int
test_get_flags(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	const SerdModelFlags flags = SERD_INDEX_OPS | SERD_INDEX_GRAPHS;
	SerdModel*           model = serd_model_new(world, flags);
	assert(serd_model_get_flags(model) == (SERD_INDEX_SPO | flags));
	serd_model_free(model);
	return 0;
}

static int
test_all_begin(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);
	SerdRange* all   = serd_model_all(model);
	SerdIter*  begin = serd_model_find(model, NULL, NULL, NULL, NULL);
	assert(serd_iter_equals(serd_range_begin(all), begin));
	assert(serd_iter_equals(serd_range_cbegin(all), begin));

	serd_range_free(all);
	serd_iter_free(begin);
	serd_model_free(model);
	return 0;
}

static int
test_add_null(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);

	serd_world_set_log_func(world, expected_error, NULL);

	assert(serd_model_add(model, 0, 0, 0, 0));
	assert(serd_model_add(model, uri(world, 1), 0, 0, 0));
	assert(serd_model_add(model, uri(world, 1), uri(world, 2), 0, 0));
	assert(serd_model_empty(model));

	serd_model_free(model);
	return 0;
}

static int
test_add_with_iterator(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);

	serd_world_set_log_func(world, expected_error, NULL);
	assert(!serd_model_add(
	        model, uri(world, 1), uri(world, 2), uri(world, 3), 0));

	// Add a statement with an active iterator
	SerdIter* iter = serd_model_begin(model);
	assert(!serd_model_add(
	        model, uri(world, 1), uri(world, 2), uri(world, 4), 0));

	// Check that iterator has been invalidated
	assert(!serd_iter_get(iter));
	assert(serd_iter_next(iter));

	serd_iter_free(iter);
	serd_model_free(model);
	return 0;
}

static int
test_erase_with_iterator(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);

	serd_world_set_log_func(world, expected_error, NULL);
	assert(!serd_model_add(
	        model, uri(world, 1), uri(world, 2), uri(world, 3), 0));
	assert(!serd_model_add(
	        model, uri(world, 4), uri(world, 5), uri(world, 6), 0));

	// Erase a statement with an active iterator
	SerdIter* iter1 = serd_model_begin(model);
	SerdIter* iter2 = serd_model_begin(model);
	assert(!serd_model_erase(model, iter1));

	// Check that erased iterator points to the next statement
	assert(serd_statement_matches(serd_iter_get(iter1),
	                              uri(world, 4),
	                              uri(world, 5),
	                              uri(world, 6),
	                              0));

	// Check that other iterator has been invalidated
	assert(!serd_iter_get(iter2));
	assert(serd_iter_next(iter2));

	serd_iter_free(iter2);
	serd_iter_free(iter1);
	serd_model_free(model);
	return 0;
}

static int
test_add_erase(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model =
	        serd_model_new(world, SERD_INDEX_SPO | SERD_INDEX_GRAPHS);

	// Add (s p "hello")
	const SerdNode* s     = uri(world, 1);
	const SerdNode* p     = uri(world, 2);
	const SerdNode* hello = manage(world, serd_new_string("hello"));
	assert(!serd_model_add(model, s, p, hello, 0));
	assert(serd_model_ask(model, s, p, hello, 0));

	// Add (s p "hi")
	const SerdNode* hi = manage(world, serd_new_string("hi"));
	assert(!serd_model_add(model, s, p, hi, NULL));
	assert(serd_model_ask(model, s, p, hi, 0));

	// Erase (s p "hi")
	SerdIter* iter = serd_model_find(model, s, p, hi, NULL);
	assert(!serd_model_erase(model, iter));
	assert(serd_model_size(model) == 1);
	serd_iter_free(iter);

	// Check that erased statement can not be found
	SerdRange* empty = serd_model_range(model, s, p, hi, NULL);
	assert(serd_range_empty(empty));
	serd_range_free(empty);

	serd_model_free(model);
	return 0;
}

static int
test_erase_all(SerdWorld* world, const size_t n_quads)
{
	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);
	generate(world, model, n_quads, NULL);

	SerdIter* iter = serd_model_begin(model);
	while (!serd_iter_equals(iter, serd_model_end(model))) {
		assert(!serd_model_erase(model, iter));
	}

	serd_iter_free(iter);
	serd_model_free(model);
	return 0;
}

static int
test_copy(SerdWorld* world, const size_t n_quads)
{
	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);
	generate(world, model, n_quads, NULL);

	SerdModel* copy = serd_model_copy(model);
	assert(serd_model_equals(model, copy));

	serd_model_free(model);
	serd_model_free(copy);
	return 0;
}

static int
test_equals(SerdWorld* world, const size_t n_quads)
{
	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);
	generate(world, model, n_quads, NULL);
	serd_model_add(
	        model, uri(world, 0), uri(world, 1), uri(world, 2), uri(world, 3));

	assert(serd_model_equals(NULL, NULL));
	assert(!serd_model_equals(NULL, model));
	assert(!serd_model_equals(model, NULL));

	SerdModel* empty = serd_model_new(world, SERD_INDEX_SPO);
	assert(!serd_model_equals(model, empty));

	SerdModel* different = serd_model_new(world, SERD_INDEX_SPO);
	generate(world, different, n_quads, NULL);
	serd_model_add(different,
	               uri(world, 1),
	               uri(world, 1),
	               uri(world, 2),
	               uri(world, 3));

	assert(serd_model_size(model) == serd_model_size(different));
	assert(!serd_model_equals(model, different));

	serd_model_free(model);
	serd_model_free(empty);
	serd_model_free(different);
	return 0;
}

static int
test_find_past_end(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel*      model = serd_model_new(world, SERD_INDEX_SPO);
	const SerdNode* s     = uri(world, 1);
	const SerdNode* p     = uri(world, 2);
	const SerdNode* o     = uri(world, 3);
	assert(!serd_model_add(model, s, p, o, 0));
	assert(serd_model_ask(model, s, p, o, 0));

	const SerdNode* huge  = uri(world, 999);
	SerdRange*      range = serd_model_range(model, huge, huge, huge, 0);
	assert(serd_range_empty(range));

	serd_range_free(range);
	serd_model_free(model);
	return 0;
}

static int
test_range(SerdWorld* world, const size_t n_quads)
{
	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);
	generate(world, model, n_quads, NULL);

	SerdRange* range1 = serd_model_all(model);
	SerdRange* range2 = serd_model_all(model);

	assert(!serd_range_empty(range1));
	assert(serd_range_empty(NULL));

	assert(!serd_range_equals(range1, NULL));
	assert(!serd_range_equals(NULL, range1));
	assert(serd_range_equals(range1, range2));

	assert(serd_iter_equals(serd_range_begin(range1),
	                        serd_range_begin(range2)));
	assert(serd_iter_equals(serd_range_cbegin(range1),
	                        serd_range_cbegin(range2)));
	assert(serd_iter_equals(serd_range_end(range1), serd_range_end(range2)));
	assert(serd_iter_equals(serd_range_cend(range1), serd_range_cend(range2)));

	assert(!serd_range_next(range2));
	assert(!serd_range_equals(range1, range2));

	serd_range_free(range2);
	serd_range_free(range1);
	serd_model_free(model);

	return 0;
}

static int
test_iter_comparison(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);

	assert(serd_iter_equals(serd_iter_copy(NULL), NULL));

	serd_world_set_log_func(world, expected_error, NULL);
	assert(!serd_model_add(
	        model, uri(world, 1), uri(world, 2), uri(world, 3), 0));

	// Add a statement with an active iterator
	SerdIter* iter1 = serd_model_begin(model);
	SerdIter* iter2 = serd_model_begin(model);
	assert(serd_iter_equals(iter1, iter2));

	serd_iter_next(iter1);
	assert(!serd_iter_equals(iter1, iter2));

	const SerdIter* end = serd_model_end(model);
	assert(serd_iter_equals(iter1, end));

	serd_iter_free(iter2);
	serd_iter_free(iter1);
	serd_model_free(model);
	return 0;
}

static int
test_triple_index_read(SerdWorld* world, const size_t n_quads)
{
	for (unsigned i = 0; i < 6; ++i) {
		SerdModel* model = serd_model_new(world, (1U << i));
		generate(world, model, n_quads, 0);
		assert(!test_read(world, model, 0, n_quads));
		serd_model_free(model);
	}

	return 0;
}

static int
test_quad_index_read(SerdWorld* world, const size_t n_quads)
{
	for (unsigned i = 0; i < 6; ++i) {
		SerdModel* model = serd_model_new(world, (1U << i) | SERD_INDEX_GRAPHS);
		const SerdNode* graph = uri(world, 42);
		generate(world, model, n_quads, graph);
		assert(!test_read(world, model, graph, n_quads));
		serd_model_free(model);
	}

	return 0;
}

static int
test_remove_graph(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model =
	        serd_model_new(world, SERD_INDEX_SPO | SERD_INDEX_GRAPHS);

	// Generate a couple of graphs
	const SerdNode* graph42 = uri(world, 42);
	const SerdNode* graph43 = uri(world, 43);
	generate(world, model, 1, graph42);
	generate(world, model, 1, graph43);

	// Remove one graph via range
	SerdRange* range = serd_model_range(model, NULL, NULL, NULL, graph43);
	SerdStatus st    = serd_model_erase_range(model, range);
	assert(!st);
	serd_range_free(range);

	// Erase the first tuple (an element in the default graph)
	SerdIter* iter = serd_model_begin(model);
	assert(!serd_model_erase(model, iter));
	serd_iter_free(iter);

	// Ensure only the other graph is left
	Quad pat = {0, 0, 0, graph42};
	for (iter = serd_model_begin(model);
	     !serd_iter_equals(iter, serd_model_end(model));
	     serd_iter_next(iter)) {
		assert(serd_statement_matches(
		        serd_iter_get(iter), pat[0], pat[1], pat[2], pat[3]));
	}
	serd_iter_free(iter);

	serd_model_free(model);
	return 0;
}

static int
test_default_graph(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model =
	        serd_model_new(world, SERD_INDEX_SPO | SERD_INDEX_GRAPHS);
	const SerdNode* s  = uri(world, 1);
	const SerdNode* p  = uri(world, 2);
	const SerdNode* o  = uri(world, 3);
	const SerdNode* g1 = uri(world, 101);
	const SerdNode* g2 = uri(world, 102);

	// Insert the same statement into two graphs
	assert(!serd_model_add(model, s, p, o, g1));
	assert(!serd_model_add(model, s, p, o, g2));

	// Ensure we only see statement once in the default graph
	assert(serd_model_count(model, s, p, o, NULL) == 1);

	serd_model_free(model);
	return 0;
}

static int
test_write_bad_list(SerdWorld* world, const size_t n_quads)
{
	(void)n_quads;

	SerdModel* model =
	        serd_model_new(world, SERD_INDEX_SPO | SERD_INDEX_GRAPHS);
	SerdNodes*      nodes   = serd_nodes_new();
	const SerdNode* s       = manage(world, serd_new_uri("urn:s"));
	const SerdNode* p       = manage(world, serd_new_uri("urn:p"));
	const SerdNode* list1   = manage(world, serd_new_blank("l1"));
	const SerdNode* list2   = manage(world, serd_new_blank("l2"));
	const SerdNode* nofirst = manage(world, serd_new_blank("nof"));
	const SerdNode* norest  = manage(world, serd_new_blank("nor"));
	const SerdNode* pfirst  = manage(world, serd_new_uri(RDF_FIRST));
	const SerdNode* prest   = manage(world, serd_new_uri(RDF_REST));
	const SerdNode* val1    = manage(world, serd_new_string("a"));
	const SerdNode* val2    = manage(world, serd_new_string("b"));

	// List where second node has no rdf:first
	serd_model_add(model, s, p, list1, NULL);
	serd_model_add(model, list1, pfirst, val1, NULL);
	serd_model_add(model, list1, prest, nofirst, NULL);

	// List where second node has no rdf:rest
	serd_model_add(model, s, p, list2, NULL);
	serd_model_add(model, list2, pfirst, val1, NULL);
	serd_model_add(model, list2, prest, norest, NULL);
	serd_model_add(model, norest, pfirst, val2, NULL);

	SerdBuffer  buffer = {NULL, 0};
	SerdEnv*    env    = serd_env_new(NULL);
	SerdWriter* writer = serd_writer_new(
	        world, SERD_TURTLE, 0, env, serd_buffer_sink, &buffer);

	SerdRange* all = serd_model_all(model);
	serd_range_serialise(all, serd_writer_get_sink(writer), 0);
	serd_range_free(all);

	serd_writer_finish(writer);
	const char* str      = serd_buffer_sink_finish(&buffer);
	const char* expected = "<urn:s>\n"
	                       "	<urn:p> (\n"
	                       "		\"a\"\n"
	                       "	) , (\n"
	                       "		\"a\"\n"
	                       "		\"b\"\n"
	                       "	) .\n";

	assert(!strcmp(str, expected));

	free(buffer.buf);
	serd_writer_free(writer);
	serd_model_free(model);
	serd_env_free(env);
	serd_nodes_free(nodes);
	return 0;
}

int
main(void)
{
	static const size_t n_quads = 300;

	serd_model_free(NULL); // Shouldn't crash

	typedef int (*TestFunc)(SerdWorld*, size_t);

	const TestFunc tests[] = {test_free_null,
	                          test_get_world,
	                          test_get_flags,
	                          test_all_begin,
	                          test_add_null,
	                          test_add_with_iterator,
	                          test_erase_with_iterator,
	                          test_add_erase,
	                          test_erase_all,
	                          test_copy,
	                          test_equals,
	                          test_find_past_end,
	                          test_range,
	                          test_iter_comparison,
	                          test_triple_index_read,
	                          test_quad_index_read,
	                          test_remove_graph,
	                          test_default_graph,
	                          test_write_bad_list,
	                          NULL};

	SerdWorld* world = serd_world_new();
	int        ret   = 0;

	for (const TestFunc* t = tests; *t; ++t) {
		serd_world_set_log_func(world, NULL, NULL);
		ret += (*t)(world, n_quads);
	}

	serd_world_free(world);
	return ret;
}
