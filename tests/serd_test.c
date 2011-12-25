/*
  Copyright 2011 David Robillard <http://drobilla.net>

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

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"

#define USTR(s) ((const uint8_t*)(s))

static bool
test_strtod(double dbl, double max_delta)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%lf", dbl);

	char* endptr = NULL;
	const double out = serd_strtod(buf, &endptr);

	const double diff = fabs(out - dbl);
	if (diff > max_delta) {
		fprintf(stderr, "error: Parsed %lf != %lf (delta %lf)\n",
		        dbl, out, diff);
		return false;
	}
	return true;
}

static SerdStatus
count_prefixes(void* handle, const SerdNode* name, const SerdNode* uri)
{
	++*(int*)handle;
	return SERD_SUCCESS;
}

static SerdStatus
count_statements(void*              handle,
                 SerdStatementFlags flags,
                 const SerdNode*    graph,
                 const SerdNode*    subject,
                 const SerdNode*    predicate,
                 const SerdNode*    object,
                 const SerdNode*    object_datatype,
                 const SerdNode*    object_lang)
{
	++*(int*)handle;
	return SERD_SUCCESS;
}

int
main()
{
	#define MAX       1000000
	#define NUM_TESTS 1000
	for (int i = 0; i < NUM_TESTS; ++i) {
		double dbl = rand() % MAX;
		dbl += (rand() % MAX) / (double)MAX;

		if (!test_strtod(dbl, 1 / (double)MAX)) {
			return 1;
		}
	}

	const double expt_test_nums[] = {
		2.0E18, -5e19, +8e20, 2e+34, -5e-5, 8e0, 9e-0, 2e+0
	};

	const char* expt_test_strs[] = {
		"02e18", "-5e019", "+8e20", "2E+34", "-5E-5", "8E0", "9e-0", " 2e+0"
	};

	for (unsigned i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
		char* endptr;
		const double num   = serd_strtod(expt_test_strs[i], &endptr);
		const double delta = fabs(num - expt_test_nums[i]);
		if (delta > DBL_EPSILON) {
			fprintf(stderr, "error: Parsed `%s' %lf != %lf (delta %lf)\n",
			        expt_test_strs[i], num, expt_test_nums[i], delta);
			return 1;
		}
	}

	// Test serd_node_new_decimal

	const double dbl_test_nums[] = {
		0.0, 42.0, .01, 2.05, -16.00001, 5.000000005, 0.0000000001
	};

	const char* dbl_test_strs[] = {
		"0.0", "42.0", "0.01", "2.05", "-16.00001", "5.00000001", "0.0"
	};

	for (unsigned i = 0; i < sizeof(dbl_test_nums) / sizeof(double); ++i) {
		SerdNode node = serd_node_new_decimal(dbl_test_nums[i], 8);
		if (strcmp((const char*)node.buf, (const char*)dbl_test_strs[i])) {
			fprintf(stderr, "error: Serialised `%s' != %s\n",
			        node.buf, dbl_test_strs[i]);
			return 1;
		}
		const size_t len = strlen((const char*)node.buf);
		if (node.n_bytes != len || node.n_chars != len) {
			fprintf(stderr, "error: Length %zu,%zu != %zu\n",
			        node.n_bytes, node.n_chars, len);
			return 1;
		}
		serd_node_free(&node);
	}

	// Test serd_node_new_integer

	const long int_test_nums[] = {
		0, -0, -23, 23, -12340, 1000, -1000
	};

	const char* int_test_strs[] = {
		"0", "0", "-23", "23", "-12340", "1000", "-1000"
	};

	for (unsigned i = 0; i < sizeof(int_test_nums) / sizeof(double); ++i) {
		fprintf(stderr, "\n*** TEST %ld\n", int_test_nums[i]);
		SerdNode node = serd_node_new_integer(int_test_nums[i]);
		if (strcmp((const char*)node.buf, (const char*)int_test_strs[i])) {
			fprintf(stderr, "error: Serialised `%s' != %s\n",
			        node.buf, int_test_strs[i]);
			return 1;
		}
		const size_t len = strlen((const char*)node.buf);
		if (node.n_bytes != len || node.n_chars != len) {
			fprintf(stderr, "error: Length %zu,%zu != %zu\n",
			        node.n_bytes, node.n_chars, len);
			return 1;
		}
		serd_node_free(&node);
	}

	// Test serd_strlen

	const uint8_t str[] = { '"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0 };

	size_t        n_bytes;
	SerdNodeFlags flags;
	size_t        len = serd_strlen(str, &n_bytes, &flags);
	if (len != 5 || n_bytes != 7
	    || flags != (SERD_HAS_QUOTE|SERD_HAS_NEWLINE)) {
		fprintf(stderr, "Bad serd_strlen(%s) len=%zu n_bytes=%zu flags=%u\n",
		        str, len, n_bytes, flags);
		return 1;
	}
	len = serd_strlen(str, NULL, &flags);
	if (len != 5) {
		fprintf(stderr, "Bad serd_strlen(%s) len=%zu flags=%u\n",
		        str, len, flags);
		return 1;
	}

	// Test serd_strerror

	const uint8_t* msg = NULL;
	if (strcmp((const char*)(msg = serd_strerror(SERD_SUCCESS)), "Success")) {
		fprintf(stderr, "Bad message `%s' for SERD_SUCCESS\n", msg);
		return 1;
	}
	for (int i = SERD_FAILURE; i <= SERD_ERR_NOT_FOUND; ++i) {
		msg = serd_strerror((SerdStatus)i);
		if (!strcmp((const char*)msg, "Success")) {
			fprintf(stderr, "Bad message `%s' for (SerdStatus)%d\n", msg, i);
			return 1;
		}
	}
	msg = serd_strerror((SerdStatus)-1);

	// Test serd_uri_to_path

	const uint8_t* uri = (const uint8_t*)"file:///home/user/foo.ttl";
	if (strcmp((const char*)serd_uri_to_path(uri), "/home/user/foo.ttl")) {
		fprintf(stderr, "Bad path %s for %s\n", serd_uri_to_path(uri), uri);
		return 1;
	}
	uri = (const uint8_t*)"file://localhost/home/user/foo.ttl";
	if (strcmp((const char*)serd_uri_to_path(uri), "/home/user/foo.ttl")) {
		fprintf(stderr, "Bad path %s for %s\n", serd_uri_to_path(uri), uri);
		return 1;
	}
	uri = (const uint8_t*)"file:illegal/file/uri";
	if (serd_uri_to_path(uri)) {
		fprintf(stderr, "Converted invalid URI `%s' to path `%s'\n",
		        uri, serd_uri_to_path(uri));
	}
	uri = (const uint8_t*)"file:///c:/awful/system";
	if (strcmp((const char*)serd_uri_to_path(uri), "c:/awful/system")) {
		fprintf(stderr, "Bad path %s for %s\n", serd_uri_to_path(uri), uri);
		return 1;
	}
	uri = (const uint8_t*)"file:///c:awful/system";
	if (strcmp((const char*)serd_uri_to_path(uri), "/c:awful/system")) {
		fprintf(stderr, "Bad path %s for %s\n", serd_uri_to_path(uri), uri);
		return 1;
	}
	uri = (const uint8_t*)"file:///0/1";
	if (strcmp((const char*)serd_uri_to_path(uri), "/0/1")) {
		fprintf(stderr, "Bad path %s for %s\n", serd_uri_to_path(uri), uri);
		return 1;
	}

	// Test serd_node_equals

	const uint8_t replacement_char_str[] = { 0xEF, 0xBF, 0xBD, 0 };
	SerdNode lhs = serd_node_from_string(SERD_LITERAL, replacement_char_str);
	SerdNode rhs = serd_node_from_string(SERD_LITERAL, USTR("123"));
	if (serd_node_equals(&lhs, &rhs)) {
		fprintf(stderr, "%s == %s\n", lhs.buf, rhs.buf);
		return 1;
	}

	SerdNode qnode = serd_node_from_string(SERD_CURIE, USTR("foo:bar"));
	if (serd_node_equals(&lhs, &qnode)) {
		fprintf(stderr, "%s == %s\n", lhs.buf, qnode.buf);
		return 1;
	}

	if (!serd_node_equals(&lhs, &lhs)) {
		fprintf(stderr, "%s != %s\n", lhs.buf, lhs.buf);
		return 1;
	}

	// Test serd_node_from_string

	SerdNode node = serd_node_from_string(SERD_LITERAL, (const uint8_t*)"hello\"");
	if (node.n_bytes != 6 || node.n_chars != 6 || node.flags != SERD_HAS_QUOTE
	    || strcmp((const char*)node.buf, "hello\"")) {
		fprintf(stderr, "Bad node %s %zu %zu %d %d\n",
		        node.buf, node.n_bytes, node.n_chars, node.flags, node.type);
		return 1;
	}

	// Test serd_node_new_uri_from_string

	SerdURI base_uri;
	SerdNode base = serd_node_new_uri_from_string(USTR("http://example.org/"),
	                                              NULL, &base_uri);
	SerdNode nil = serd_node_new_uri_from_string(NULL, &base_uri, NULL);
	if (nil.type != SERD_URI || strcmp((const char*)nil.buf, (const char*)base.buf)) {
		fprintf(stderr, "URI %s != base %s\n", nil.buf, base.buf);
		return 1;
	}
	serd_node_free(&base);
	serd_node_free(&nil);
	
	// Test SerdEnv

	SerdNode u   = serd_node_from_string(SERD_URI, USTR("http://example.org/foo"));
	SerdNode b   = serd_node_from_string(SERD_CURIE, USTR("invalid"));
	SerdNode c   = serd_node_from_string(SERD_CURIE, USTR("eg.2:b"));
	SerdEnv* env = serd_env_new(NULL);
	serd_env_set_prefix_from_strings(env, USTR("eg.2"), USTR("http://example.org/"));

	if (!serd_env_set_base_uri(env, &node)) {
		fprintf(stderr, "Set base URI to %s\n", node.buf);
		return 1;
	}

	SerdChunk prefix, suffix;
	if (!serd_env_expand(env, &b, &prefix, &suffix)) {
		fprintf(stderr, "Expanded invalid curie %s\n", b.buf);
		return 1;
	}

	SerdNode xnode = serd_env_expand_node(env, &node);
	if (!serd_node_equals(&xnode, &SERD_NODE_NULL)) {
		fprintf(stderr, "Expanded %s to %s\n", c.buf, xnode.buf);
		return 1;
	}

	SerdNode xu = serd_env_expand_node(env, &u);
	if (strcmp((const char*)xu.buf, "http://example.org/foo")) {
		fprintf(stderr, "Expanded %s to %s\n", c.buf, xu.buf);
		return 1;
	}
	serd_node_free(&xu);

	SerdNode badpre = serd_node_from_string(SERD_CURIE, USTR("hm:what"));
	SerdNode xbadpre = serd_env_expand_node(env, &badpre);
	if (!serd_node_equals(&xbadpre, &SERD_NODE_NULL)) {
		fprintf(stderr, "Expanded invalid curie %s\n", badpre.buf);
		return 1;
	}

	SerdNode xc = serd_env_expand_node(env, &c);
	if (strcmp((const char*)xc.buf, "http://example.org/b")) {
		fprintf(stderr, "Expanded %s to %s\n", c.buf, xc.buf);
		return 1;
	}
	serd_node_free(&xc);

	if (!serd_env_set_prefix(env, &SERD_NODE_NULL, &SERD_NODE_NULL)) {
		fprintf(stderr, "Set NULL prefix\n");
		return 1;
	}

	const SerdNode lit = serd_node_from_string(SERD_LITERAL, USTR("hello"));
	if (!serd_env_set_prefix(env, &b, &lit)) {
		fprintf(stderr, "Set prefix to literal\n");
		return 1;
	}

	int n_prefixes = 0;
	serd_env_set_prefix_from_strings(env, USTR("eg.2"), USTR("http://example.org/"));
	serd_env_foreach(env, count_prefixes, &n_prefixes);
	if (n_prefixes != 1) {
		fprintf(stderr, "Bad prefix count %d\n", n_prefixes);
		return 1;
	}

	SerdNode shorter_uri = serd_node_from_string(SERD_URI, USTR("urn:foo"));
	SerdNode prefix_name;
	if (serd_env_qualify(env, &shorter_uri, &prefix_name, &suffix)) {
		fprintf(stderr, "Qualified %s\n", shorter_uri.buf);
		return 1;
	}

	// Test SerdReader and SerdWriter

	const char* path = tmpnam(NULL);
	FILE* fd = fopen(path, "w");
	if (!fd) {
		fprintf(stderr, "Failed to open file %s\n", path);
		return 1;
	}

	int* n_statements = malloc(sizeof(int));
	*n_statements = 0;

	SerdWriter* writer = serd_writer_new(
		SERD_TURTLE, 0, env, NULL, serd_file_sink, fd);
	if (!writer) {
		fprintf(stderr, "Failed to create writer\n");
		return 1;
	}

	serd_writer_chop_blank_prefix(writer, USTR("tmp"));
	serd_writer_chop_blank_prefix(writer, NULL);

	if (!serd_writer_set_base_uri(writer, &lit)) {
		fprintf(stderr, "Set base URI to %s\n", lit.buf);
		return 1;
	}

	if (!serd_writer_set_prefix(writer, &lit, &lit)) {
		fprintf(stderr, "Set prefix %s to %s\n", lit.buf, lit.buf);
		return 1;
	}

	if (!serd_writer_end_anon(writer, NULL)) {
		fprintf(stderr, "Ended non-existent anonymous node\n");
		return 1;
	}

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
	                              { NULL, NULL, NULL, NULL, NULL } };
	for (unsigned i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 5); ++i) {
		if (!serd_writer_write_statement(
			    writer, 0, NULL,
			    junk[i][0], junk[i][1], junk[i][2], junk[i][3], junk[i][4])) {
			fprintf(stderr, "Successfully wrote junk statement %d\n", i);
		return 1;
		}
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
	for (unsigned i = 0; i < sizeof(good) / (sizeof(SerdNode*) * 5); ++i) {
		if (serd_writer_write_statement(
			    writer, 0, NULL,
			    good[i][0], good[i][1], good[i][2], good[i][3], good[i][4])) {
			fprintf(stderr, "Failed to write good statement %d\n", i);
			return 1;
		}
	}

	// Write 1 statement with bad UTF-8 (should be replaced)
	if (serd_writer_write_statement(writer, 0, NULL,
	                                &s, &p, &o, NULL, NULL)) {
		fprintf(stderr, "Failed to write junk UTF-8\n");
		return 1;
	}

	// Write 1 valid statement
	o = serd_node_from_string(SERD_LITERAL, USTR("hello"));
	if (serd_writer_write_statement(writer, 0, NULL,
	                                &s, &p, &o, NULL, NULL)) {
		fprintf(stderr, "Failed to write valid statement\n");
		return 1;
	}

	serd_writer_free(writer);
	fseek(fd, 0, SEEK_SET);

	SerdReader* reader = serd_reader_new(
		SERD_TURTLE, n_statements, free,
		NULL, NULL, count_statements, NULL);
	if (!reader) {
		fprintf(stderr, "Failed to create reader\n");
		return 1;
	}
	if (serd_reader_get_handle(reader) != n_statements) {
		fprintf(stderr, "Corrupt reader handle\n");
		return 1;
	}

	serd_reader_add_blank_prefix(reader, USTR("tmp"));
	serd_reader_add_blank_prefix(reader, NULL);

	if (!serd_reader_read_file(reader, USTR("http://notafile"))) {
		fprintf(stderr, "Apparently read an http URI\n");
		return 1;
	}
	if (!serd_reader_read_file(reader, USTR("file:///better/not/exist"))) {
		fprintf(stderr, "Apprently read a non-existent file\n");
		return 1;
	}
	SerdStatus st = serd_reader_read_file(reader, USTR(path));
	if (st) {
		fprintf(stderr, "Error reading file (%s)\n", serd_strerror(st));
		return 1;
	}

	if (*n_statements != 12) {
		fprintf(stderr, "Bad statement count %d\n", *n_statements);
		return 1;
	}

	if (!serd_reader_read_string(reader, USTR("This isn't Turtle at all."))) {
		fprintf(stderr, "Parsed invalid string successfully.\n");
		return 1;
	}

	serd_reader_free(reader);
	fclose(fd);

	serd_env_free(env);

	printf("Success\n");
	return 0;
}
