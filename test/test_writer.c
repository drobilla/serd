// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

static void
test_write_long_literal(void)
{
  SerdEnv*    env    = serd_env_new(NULL);
  SerdChunk   chunk  = {NULL, 0};
  SerdWriter* writer = serd_writer_new(
    SERD_TURTLE, (SerdStyle)0, env, NULL, serd_chunk_sink, &chunk);

  assert(writer);

  SerdNode s = serd_node_from_string(SERD_URI, USTR("http://example.org/s"));
  SerdNode p = serd_node_from_string(SERD_URI, USTR("http://example.org/p"));
  SerdNode o =
    serd_node_from_string(SERD_LITERAL, USTR("hello \"\"\"world\"\"\"!"));

  assert(!serd_writer_write_statement(writer, 0, NULL, &s, &p, &o, NULL, NULL));

  serd_writer_free(writer);
  serd_env_free(env);

  uint8_t* out = serd_chunk_sink_finish(&chunk);

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

  assert(!strcmp((char*)out, expected));
  serd_free(out);
}

static void
test_write_nested_anon(void)
{
  SerdEnv*    env    = serd_env_new(NULL);
  SerdChunk   chunk  = {NULL, 0};
  SerdWriter* writer = serd_writer_new(
    SERD_TURTLE, (SerdStyle)0, env, NULL, serd_chunk_sink, &chunk);

  assert(writer);

  SerdNode s0  = serd_node_from_string(SERD_URI, USTR("http://example.org/s0"));
  SerdNode p0  = serd_node_from_string(SERD_URI, USTR("http://example.org/p0"));
  SerdNode b0  = serd_node_from_string(SERD_BLANK, USTR("b0"));
  SerdNode p1  = serd_node_from_string(SERD_URI, USTR("http://example.org/p1"));
  SerdNode b1  = serd_node_from_string(SERD_BLANK, USTR("b1"));
  SerdNode p2  = serd_node_from_string(SERD_URI, USTR("http://example.org/p2"));
  SerdNode o2  = serd_node_from_string(SERD_URI, USTR("http://example.org/o2"));
  SerdNode p3  = serd_node_from_string(SERD_URI, USTR("http://example.org/p3"));
  SerdNode p4  = serd_node_from_string(SERD_URI, USTR("http://example.org/p4"));
  SerdNode o4  = serd_node_from_string(SERD_URI, USTR("http://example.org/o4"));
  SerdNode nil = serd_node_from_string(
    SERD_URI, USTR("http://www.w3.org/1999/02/22-rdf-syntax-ns#nil"));

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_O_BEGIN, NULL, &s0, &p0, &b0, NULL, NULL));

  assert(!serd_writer_write_statement(writer,
                                      SERD_ANON_O_BEGIN | SERD_ANON_CONT,
                                      NULL,
                                      &b0,
                                      &p1,
                                      &b1,
                                      NULL,
                                      NULL));

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_CONT, NULL, &b1, &p2, &o2, NULL, NULL));

  assert(!serd_writer_write_statement(writer,
                                      SERD_ANON_CONT | SERD_LIST_O_BEGIN,
                                      NULL,
                                      &b1,
                                      &p3,
                                      &nil,
                                      NULL,
                                      NULL));

  assert(!serd_writer_end_anon(writer, &b1));
  assert(!serd_writer_write_statement(
    writer, SERD_ANON_CONT, NULL, &b0, &p4, &o4, NULL, NULL));

  assert(!serd_writer_end_anon(writer, &b0));

  serd_writer_free(writer);
  serd_env_free(env);

  uint8_t* const out = serd_chunk_sink_finish(&chunk);

  static const char* const expected =
    "<http://example.org/s0>\n"
    "\t<http://example.org/p0> [\n"
    "\t\t<http://example.org/p1> [\n"
    "\t\t\t<http://example.org/p2> <http://example.org/o2> ;\n"
    "\t\t\t<http://example.org/p3> ()\n"
    "\t\t] ;\n"
    "\t\t<http://example.org/p4> <http://example.org/o4>\n"
    "\t] .\n";

  fprintf(stderr, "%s\n", out);
  assert(!strcmp((char*)out, expected));
  serd_free(out);
}

static size_t
null_sink(const void* const buf, const size_t len, void* const stream)
{
  (void)buf;
  (void)stream;

  return len;
}

static void
test_writer_cleanup(void)
{
  SerdStatus  st  = SERD_SUCCESS;
  SerdEnv*    env = serd_env_new(NULL);
  SerdWriter* writer =
    serd_writer_new(SERD_TURTLE, (SerdStyle)0U, env, NULL, null_sink, NULL);

  SerdNode s = serd_node_from_string(SERD_URI, USTR("http://example.org/s"));
  SerdNode p = serd_node_from_string(SERD_URI, USTR("http://example.org/p"));

  char     o_buf[12] = {'b', '0', '\0'};
  SerdNode o         = serd_node_from_string(SERD_BLANK, USTR(o_buf));

  st = serd_writer_write_statement(
    writer, SERD_ANON_O_BEGIN, NULL, &s, &p, &o, NULL, NULL);

  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 1U; !st && i < 9U; ++i) {
    char next_o_buf[12] = {'\0'};
    snprintf(next_o_buf, sizeof(next_o_buf), "b%u", i);

    SerdNode next_o = serd_node_from_string(SERD_BLANK, USTR(next_o_buf));

    st = serd_writer_write_statement(writer,
                                     SERD_ANON_O_BEGIN | SERD_ANON_CONT,
                                     NULL,
                                     &o,
                                     &p,
                                     &next_o,
                                     NULL,
                                     NULL);

    assert(!st);

    memcpy(o_buf, next_o_buf, sizeof(o_buf));
  }

  // Finish writing without terminating nodes
  assert(!(st = serd_writer_finish(writer)));

  // Set the base to an empty URI
  assert(!(st = serd_writer_set_base_uri(writer, NULL)));

  // Free (which could leak if the writer doesn't clean up the stack properly)
  serd_writer_free(writer);
  serd_env_free(env);
}

static void
test_strict_write(void)
{
  const char* const path = "serd_strict_write_test.ttl";
  FILE* const       fd   = fopen(path, "wb");
  assert(fd);

  SerdEnv* const    env    = serd_env_new(NULL);
  SerdWriter* const writer = serd_writer_new(
    SERD_TURTLE, (SerdStyle)SERD_STYLE_STRICT, env, NULL, null_sink, fd);

  assert(writer);

  const uint8_t bad_str[] = {0xFF, 0x90, 'h', 'i', 0};

  SerdNode s = serd_node_from_string(SERD_URI, USTR("http://example.org/s"));
  SerdNode p = serd_node_from_string(SERD_URI, USTR("http://example.org/p"));

  SerdNode bad_lit = serd_node_from_string(SERD_LITERAL, bad_str);
  SerdNode bad_uri = serd_node_from_string(SERD_URI, bad_str);

  assert(serd_writer_write_statement(
           writer, 0, NULL, &s, &p, &bad_lit, NULL, NULL) == SERD_ERR_BAD_TEXT);

  assert(serd_writer_write_statement(
           writer, 0, NULL, &s, &p, &bad_uri, NULL, NULL) == SERD_ERR_BAD_TEXT);

  serd_writer_free(writer);
  serd_env_free(env);
  fclose(fd);
  remove(path);
}

// Produce a write error without setting errno
static size_t
error_sink(const void* const buf, const size_t len, void* const stream)
{
  (void)buf;
  (void)len;
  (void)stream;
  return 0U;
}

static void
test_write_error(void)
{
  SerdEnv* const env    = serd_env_new(NULL);
  SerdWriter*    writer = NULL;
  SerdStatus     st     = SERD_SUCCESS;

  SerdNode u = serd_node_from_string(SERD_URI, USTR("http://example.com/u"));

  writer =
    serd_writer_new(SERD_TURTLE, (SerdStyle)0, env, NULL, error_sink, NULL);
  assert(writer);
  st = serd_writer_write_statement(writer, 0U, NULL, &u, &u, &u, NULL, NULL);
  assert(st == SERD_ERR_BAD_WRITE);
  serd_writer_free(writer);

  serd_env_free(env);
}

int
main(void)
{
  test_write_long_literal();
  test_write_nested_anon();
  test_writer_cleanup();
  test_strict_write();
  test_write_error();

  return 0;
}
