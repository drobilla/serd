/*
  Copyright 2011-2020 David Robillard <http://drobilla.net>

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
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

#ifndef INFINITY
#    define INFINITY (DBL_MAX + DBL_MAX)
#endif
#ifndef NAN
#    define NAN (INFINITY - INFINITY)
#endif

static void
test_strtod(double dbl, double max_delta)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%f", dbl);

	char* endptr = NULL;
	const double out = serd_strtod(buf, &endptr);

	const double diff = fabs(out - dbl);
	assert(diff <= max_delta);
}

typedef struct {
	int             n_statements;
	const SerdNode* graph;
} ReaderTest;

static SerdStatus
test_sink(void*              handle,
          SerdStatementFlags flags,
          const SerdNode*    graph,
          const SerdNode*    subject,
          const SerdNode*    predicate,
          const SerdNode*    object,
          const SerdNode*    object_datatype,
          const SerdNode*    object_lang)
{
	(void)flags;
	(void)subject;
	(void)predicate;
	(void)object;
	(void)object_datatype;
	(void)object_lang;

	ReaderTest* rt = (ReaderTest*)handle;
	++rt->n_statements;
	rt->graph = graph;
	return SERD_SUCCESS;
}

/// Returns EOF after a statement, then succeeds again (like a socket)
static size_t
eof_test_read(void* buf, size_t size, size_t nmemb, void* stream)
{
	assert(nmemb == 1);

	static const char* const string = "_:s1 <http://example.org/p> _:o1 .\n"
	                                  "_:s2 <http://example.org/p> _:o2 .\n";

	size_t* count = (size_t*)stream;
	if (*count == 34 || *count == 35 || *count + nmemb >= strlen(string)) {
		++*count;
		return 0;
	}

	memcpy((char*)buf, string + *count, size * nmemb);
	*count += nmemb;
	return nmemb;
}

static int
eof_test_error(void* stream)
{
	(void)stream;
	return 0;
}

static void
test_read_chunks(void)
{
	ReaderTest* const rt   = (ReaderTest*)calloc(1, sizeof(ReaderTest));
	FILE* const       f    = tmpfile();
	static const char null = 0;
	SerdReader* const reader =
	        serd_reader_new(SERD_TURTLE, rt, free, NULL, NULL, test_sink, NULL);

	assert(reader);
	assert(serd_reader_get_handle(reader) == rt);
	assert(f);

	SerdStatus st = serd_reader_start_stream(reader, f, NULL, false);
	assert(st == SERD_SUCCESS);

	// Write two statement separated by null characters
	fprintf(f, "@prefix eg: <http://example.org/> .\n");
	fprintf(f, "eg:s eg:p eg:o1 .\n");
	fwrite(&null, sizeof(null), 1, f);
	fprintf(f, "eg:s eg:p eg:o2 .\n");
	fwrite(&null, sizeof(null), 1, f);
	fseek(f, 0, SEEK_SET);

	// Read prefix
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_SUCCESS);
	assert(rt->n_statements == 0);

	// Read first statement
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_SUCCESS);
	assert(rt->n_statements == 1);

	// Read terminator
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_FAILURE);
	assert(rt->n_statements == 1);

	// Read second statement (after null terminator)
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_SUCCESS);
	assert(rt->n_statements == 2);

	// Read terminator
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_FAILURE);
	assert(rt->n_statements == 2);

	// EOF
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_FAILURE);
	assert(rt->n_statements == 2);

	serd_reader_free(reader);
	fclose(f);
}

static void
test_string_to_double(void)
{
	const double expt_test_nums[] = {
		2.0E18, -5e19, +8e20, 2e+24, -5e-5, 8e0, 9e-0, 2e+0
	};

	const char* expt_test_strs[] = {
		"02e18", "-5e019", "+8e20", "2E+24", "-5E-5", "8E0", "9e-0", " 2e+0"
	};

	for (size_t i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
		const double num   = serd_strtod(expt_test_strs[i], NULL);
		const double delta = fabs(num - expt_test_nums[i]);
		assert(delta <= DBL_EPSILON);

		test_strtod(expt_test_nums[i], DBL_EPSILON);
	}
}

static void
test_double_to_node(void)
{
	const double dbl_test_nums[] = {
		0.0, 9.0, 10.0, .01, 2.05, -16.00001, 5.000000005, 0.0000000001, NAN, INFINITY
	};

	const char* dbl_test_strs[] = {
		"0.0", "9.0", "10.0", "0.01", "2.05", "-16.00001", "5.00000001", "0.0", NULL, NULL
	};

	for (size_t i = 0; i < sizeof(dbl_test_nums) / sizeof(double); ++i) {
		SerdNode   node = serd_node_new_decimal(dbl_test_nums[i], 8);
		const bool pass = (node.buf && dbl_test_strs[i])
			? !strcmp((const char*)node.buf, dbl_test_strs[i])
			: ((const char*)node.buf == dbl_test_strs[i]);
		assert(pass);
		const size_t len = node.buf ? strlen((const char*)node.buf) : 0;
		assert(node.n_bytes == len && node.n_chars == len);
		serd_node_free(&node);
	}
}

static void
test_integer_to_node(void)
{
	const long int_test_nums[] = {
		0, -0, -23, 23, -12340, 1000, -1000
	};

	const char* int_test_strs[] = {
		"0", "0", "-23", "23", "-12340", "1000", "-1000"
	};

	for (size_t i = 0; i < sizeof(int_test_nums) / sizeof(double); ++i) {
		SerdNode node = serd_node_new_integer(int_test_nums[i]);
		assert(!strcmp((const char*)node.buf, (const char*)int_test_strs[i]));
		const size_t len = strlen((const char*)node.buf);
		assert(node.n_bytes == len && node.n_chars == len);
		serd_node_free(&node);
	}
}

static void
test_blob_to_node(void)
{
	for (size_t size = 0; size < 256; ++size) {
		uint8_t* data = size > 0 ? (uint8_t*)malloc(size) : NULL;
		for (size_t i = 0; i < size; ++i) {
			data[i] = (uint8_t)((size + i) % 256);
		}

		SerdNode blob = serd_node_new_blob(data, size, size % 5);

		assert(blob.n_bytes == blob.n_chars);
		assert(blob.n_bytes == strlen((const char*)blob.buf));

		size_t   out_size = 0;
		uint8_t* out = (uint8_t*)serd_base64_decode(
			blob.buf, blob.n_bytes, &out_size);
		assert(out_size == size);

		for (size_t i = 0; i < size; ++i) {
			assert(out[i] == data[i]);
		}

		serd_node_free(&blob);
		serd_free(out);
		free(data);
	}
}

static void
test_strlen(void)
{
	const uint8_t str[] = { '"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0 };

	size_t        n_bytes = 0;
	SerdNodeFlags flags   = 0;
	size_t        len     = serd_strlen(str, &n_bytes, &flags);
	assert(len == 5 && n_bytes == 7 &&
	       flags == (SERD_HAS_QUOTE | SERD_HAS_NEWLINE));
	len = serd_strlen(str, NULL, &flags);
	assert(len == 5);

	assert(serd_strlen(str, &n_bytes, NULL) == 5);
}

static void
test_strerror(void)
{
	const uint8_t* msg = serd_strerror(SERD_SUCCESS);
	assert(!strcmp((const char*)msg, "Success"));
	for (int i = SERD_FAILURE; i <= SERD_ERR_INTERNAL; ++i) {
		msg = serd_strerror((SerdStatus)i);
		assert(strcmp((const char*)msg, "Success"));
	}

	msg = serd_strerror((SerdStatus)-1);
	assert(!strcmp((const char*)msg, "Unknown error"));
}

static void
test_node_equals(void)
{
	const uint8_t replacement_char_str[] = { 0xEF, 0xBF, 0xBD, 0 };
	SerdNode lhs = serd_node_from_string(SERD_LITERAL, replacement_char_str);
	SerdNode rhs = serd_node_from_string(SERD_LITERAL, USTR("123"));
	assert(!serd_node_equals(&lhs, &rhs));

	SerdNode qnode = serd_node_from_string(SERD_CURIE, USTR("foo:bar"));
	assert(!serd_node_equals(&lhs, &qnode));
	assert(serd_node_equals(&lhs, &lhs));

	SerdNode null_copy = serd_node_copy(&SERD_NODE_NULL);
	assert(serd_node_equals(&SERD_NODE_NULL, &null_copy));
}

static void
test_node_from_string(void)
{
	SerdNode node = serd_node_from_string(SERD_LITERAL, (const uint8_t*)"hello\"");
	assert(node.n_bytes == 6 && node.n_chars == 6 &&
	       node.flags == SERD_HAS_QUOTE &&
	       !strcmp((const char*)node.buf, "hello\""));

	node = serd_node_from_string(SERD_URI, NULL);
	assert(serd_node_equals(&node, &SERD_NODE_NULL));
}

static void
test_node_from_substring(void)
{
	SerdNode empty = serd_node_from_substring(SERD_LITERAL, NULL, 32);
	assert(!empty.buf && !empty.n_bytes && !empty.n_chars && !empty.flags &&
	       !empty.type);

	SerdNode a_b = serd_node_from_substring(SERD_LITERAL, USTR("a\"bc"), 3);
	assert(a_b.n_bytes == 3 && a_b.n_chars == 3 &&
	       a_b.flags == SERD_HAS_QUOTE &&
	       !strncmp((const char*)a_b.buf, "a\"b", 3));

	a_b = serd_node_from_substring(SERD_LITERAL, USTR("a\"bc"), 10);
	assert(a_b.n_bytes == 4 && a_b.n_chars == 4 &&
	       a_b.flags == SERD_HAS_QUOTE &&
	       !strncmp((const char*)a_b.buf, "a\"bc", 4));
}

static void
test_writer(const char* const path)
{
	FILE* fd = fopen(path, "wb");
	SerdEnv* env = serd_env_new(NULL);
	assert(fd);

	SerdWriter* writer = serd_writer_new(
		SERD_TURTLE, (SerdStyle)0, env, NULL, serd_file_sink, fd);
	assert(writer);

	serd_writer_chop_blank_prefix(writer, USTR("tmp"));
	serd_writer_chop_blank_prefix(writer, NULL);

	const SerdNode lit = serd_node_from_string(SERD_LITERAL, USTR("hello"));

	assert(serd_writer_set_base_uri(writer, &lit));
	assert(serd_writer_set_prefix(writer, &lit, &lit));
	assert(serd_writer_end_anon(writer, NULL));
	assert(serd_writer_get_env(writer) == env);

	uint8_t buf[] = { 0x80, 0, 0, 0, 0 };
	SerdNode s = serd_node_from_string(SERD_URI, USTR(""));
	SerdNode p = serd_node_from_string(SERD_URI, USTR("http://example.org/pred"));
	SerdNode o = serd_node_from_string(SERD_LITERAL, buf);

	// Write 3 invalid statements (should write nothing)
	const SerdNode* junk[][5] = { { &s, &p, NULL, NULL, NULL },
	                              { &s, NULL, &o, NULL, NULL },
	                              { NULL, &p, &o, NULL, NULL },
	                              { &s, &p, &SERD_NODE_NULL, NULL, NULL },
	                              { &s, &SERD_NODE_NULL, &o, NULL, NULL },
	                              { &SERD_NODE_NULL, &p, &o, NULL, NULL },
	                              { &s, &o, &o, NULL, NULL },
	                              { &o, &p, &o, NULL, NULL },
	                              { &s, &p, &SERD_NODE_NULL, NULL, NULL },
	                              { NULL, NULL, NULL, NULL, NULL } };
	for (size_t i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 5); ++i) {
		assert(serd_writer_write_statement(
			       writer, 0, NULL,
			       junk[i][0], junk[i][1], junk[i][2], junk[i][3], junk[i][4]));
	}

	const SerdNode t = serd_node_from_string(SERD_URI, USTR("urn:Type"));
	const SerdNode l = serd_node_from_string(SERD_LITERAL, USTR("en"));
	const SerdNode* good[][5] = { { &s, &p, &o, NULL, NULL },
	                              { &s, &p, &o, &SERD_NODE_NULL, &SERD_NODE_NULL },
	                              { &s, &p, &o, &t, NULL },
	                              { &s, &p, &o, NULL, &l },
	                              { &s, &p, &o, &t, &l },
	                              { &s, &p, &o, &t, &SERD_NODE_NULL },
	                              { &s, &p, &o, &SERD_NODE_NULL, &l },
	                              { &s, &p, &o, NULL, &SERD_NODE_NULL },
	                              { &s, &p, &o, &SERD_NODE_NULL, NULL },
	                              { &s, &p, &o, &SERD_NODE_NULL, NULL } };
	for (size_t i = 0; i < sizeof(good) / (sizeof(SerdNode*) * 5); ++i) {
		assert(!serd_writer_write_statement(
			       writer, 0, NULL,
			       good[i][0], good[i][1], good[i][2], good[i][3], good[i][4]));
	}

	// Write statements with bad UTF-8 (should be replaced)
	const uint8_t bad_str[] = { 0xFF, 0x90, 'h', 'i', 0 };
	SerdNode      bad_lit   = serd_node_from_string(SERD_LITERAL, bad_str);
	SerdNode      bad_uri   = serd_node_from_string(SERD_URI, bad_str);
	assert(!serd_writer_write_statement(writer, 0, NULL,
	                                    &s, &p, &bad_lit, NULL, NULL));
	assert(!serd_writer_write_statement(writer, 0, NULL,
	                                    &s, &p, &bad_uri, NULL, NULL));

	// Write 1 valid statement
	o = serd_node_from_string(SERD_LITERAL, USTR("hello"));
	assert(!serd_writer_write_statement(writer, 0, NULL,
	                                    &s, &p, &o, NULL, NULL));

	serd_writer_free(writer);

	// Test chunk sink
	SerdChunk chunk = { NULL, 0 };
	writer = serd_writer_new(
		SERD_TURTLE, (SerdStyle)0, env, NULL, serd_chunk_sink, &chunk);

	o = serd_node_from_string(SERD_URI, USTR("http://example.org/base"));
	assert(!serd_writer_set_base_uri(writer, &o));

	serd_writer_free(writer);
	uint8_t* out = serd_chunk_sink_finish(&chunk);

	assert(!strcmp((const char*)out, "@base <http://example.org/base> .\n"));
	serd_free(out);

	// Test writing empty node
	SerdNode    nothing = serd_node_from_string(SERD_NOTHING, USTR(""));
	FILE* const empty   = tmpfile();

	writer = serd_writer_new(
		SERD_TURTLE, (SerdStyle)0, env, NULL, serd_file_sink, empty);

	// FIXME: error handling
	serd_writer_write_statement(writer, 0, NULL,
	                            &s, &p, &nothing, NULL, NULL);

	assert((size_t)ftell(empty) == strlen("<>\n\t<http://example.org/pred> "));

	serd_writer_free(writer);
	fclose(empty);

	serd_env_free(env);
	fclose(fd);
}

static void
test_reader(const char* path)
{
	ReaderTest* rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
	SerdReader* reader = serd_reader_new(
		SERD_TURTLE, rt, free,
		NULL, NULL, test_sink, NULL);
	assert(reader);
	assert(serd_reader_get_handle(reader) == rt);

	SerdNode g = serd_node_from_string(SERD_URI, USTR("http://example.org/"));
	serd_reader_set_default_graph(reader, &g);
	serd_reader_add_blank_prefix(reader, USTR("tmp"));
	serd_reader_add_blank_prefix(reader, NULL);

	assert(serd_reader_read_file(reader, USTR("http://notafile")));
	assert(serd_reader_read_file(reader, USTR("file:///better/not/exist")));
	assert(serd_reader_read_file(reader, USTR("file://")));

	const SerdStatus st = serd_reader_read_file(reader, USTR(path));
	assert(!st);
	assert(rt->n_statements == 13);
	assert(rt->graph && rt->graph->buf &&
	       !strcmp((const char*)rt->graph->buf, "http://example.org/"));

	assert(serd_reader_read_string(reader, USTR("This isn't Turtle at all.")));

	// A read of a big page hits EOF then fails to read chunks immediately
	{
		FILE* temp = tmpfile();
		assert(temp);
		fprintf(temp, "_:s <http://example.org/p> _:o .\n");
		fflush(temp);
		fseek(temp, 0L, SEEK_SET);

		serd_reader_start_stream(reader, temp, NULL, true);

		assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

		serd_reader_end_stream(reader);
		fclose(temp);
	}

	// A byte-wise reader that hits EOF once then continues (like a socket)
	{
		size_t n_reads = 0;
		serd_reader_start_source_stream(reader,
		                                (SerdSource)eof_test_read,
		                                (SerdStreamErrorFunc)eof_test_error,
		                                &n_reads,
		                                NULL,
		                                1);

		assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
		assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
	}

	serd_reader_free(reader);
}

int
main(void)
{
	test_string_to_double();
	test_double_to_node();
	test_integer_to_node();
	test_blob_to_node();
	test_strlen();
	test_strerror();
	test_node_equals();
	test_node_from_string();
	test_node_from_substring();
	test_read_chunks();

	const char* const path = "serd_test.ttl";
	test_writer(path);
	test_reader(path);

	printf("Success\n");
	return 0;
}
