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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

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
test_read_string(void)
{
	ReaderTest* rt = (ReaderTest*)calloc(1, sizeof(ReaderTest));
	SerdReader* reader =
	    serd_reader_new(SERD_TURTLE, rt, free, NULL, NULL, test_sink, NULL);

	assert(reader);
	assert(serd_reader_get_handle(reader) == rt);

	// Test reading a string that ends exactly at the end of input (no newline)
	const SerdStatus st =
		serd_reader_read_string(reader,
		                        USTR("<http://example.org/s> <http://example.org/p> "
		                             "<http://example.org/o> ."));

	assert(!st);
	assert(rt->n_statements == 1);

	serd_reader_free(reader);
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
	const SerdNode* junk[][5] = { { &s, &p, &SERD_NODE_NULL, NULL, NULL },
	                              { &s, &SERD_NODE_NULL, &o, NULL, NULL },
	                              { &SERD_NODE_NULL, &p, &o, NULL, NULL },
	                              { &s, &o, &o, NULL, NULL },
	                              { &o, &p, &o, NULL, NULL },
	                              { &s, &p, &SERD_NODE_NULL, NULL, NULL } };
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

#if defined(__GNUC__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wnonnull"
#endif
	serd_reader_add_blank_prefix(reader, NULL);
#if defined(__GNUC__)
#	pragma GCC diagnostic pop
#endif

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
	test_read_chunks();
	test_read_string();

	const char* const path = "serd_test.ttl";
	test_writer(path);
	test_reader(path);

	printf("Success\n");
	return 0;
}
