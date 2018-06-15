// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/memory.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/syntax.h"
#include "serd/writer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
test_write_bad_prefix(void)
{
  SerdEnv*    env    = serd_env_new(serd_empty_string());
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer =
    serd_writer_new(SERD_TURTLE, 0U, env, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* name = serd_new_string(serd_string("eg"));
  SerdNode* uri  = serd_new_uri(serd_string("rel"));

  assert(serd_writer_set_prefix(writer, name, uri) == SERD_BAD_ARG);

  char* const out = serd_buffer_sink_finish(&buffer);

  assert(!strcmp(out, ""));
  serd_free(out);

  serd_node_free(uri);
  serd_node_free(name);
  serd_writer_free(writer);
  serd_env_free(env);
}

static void
test_write_long_literal(void)
{
  SerdEnv*    env    = serd_env_new(serd_empty_string());
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer =
    serd_writer_new(SERD_TURTLE, 0U, env, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/p"));
  SerdNode* o = serd_new_string(serd_string("hello \"\"\"world\"\"\"!"));

  assert(!serd_writer_write_statement(writer, 0, NULL, s, p, o));

  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);

  char* out = serd_buffer_sink_finish(&buffer);

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

  assert(!strcmp((char*)out, expected));
  serd_free(out);
}

static void
test_write_nested_anon(void)
{
  SerdEnv*    env    = serd_env_new(serd_empty_string());
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer =
    serd_writer_new(SERD_TURTLE, 0U, env, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* s0 = serd_new_uri(serd_string("http://example.org/s0"));
  SerdNode* p0 = serd_new_uri(serd_string("http://example.org/p0"));
  SerdNode* b0 = serd_new_blank(serd_string("b0"));
  SerdNode* p1 = serd_new_uri(serd_string("http://example.org/p1"));
  SerdNode* b1 = serd_new_blank(serd_string("b1"));
  SerdNode* p2 = serd_new_uri(serd_string("http://example.org/p2"));
  SerdNode* o2 = serd_new_uri(serd_string("http://example.org/o2"));
  SerdNode* p3 = serd_new_uri(serd_string("http://example.org/p3"));
  SerdNode* p4 = serd_new_uri(serd_string("http://example.org/p4"));
  SerdNode* o4 = serd_new_uri(serd_string("http://example.org/o4"));
  SerdNode* nil =
    serd_new_uri(serd_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#nil"));

  assert(
    !serd_writer_write_statement(writer, SERD_ANON_O_BEGIN, NULL, s0, p0, b0));

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_O_BEGIN | SERD_ANON_CONT, NULL, b0, p1, b1));

  assert(
    !serd_writer_write_statement(writer, SERD_ANON_CONT, NULL, b1, p2, o2));

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_CONT | SERD_LIST_O_BEGIN, NULL, b1, p3, nil));

  assert(!serd_writer_end_anon(writer, b1));
  assert(
    !serd_writer_write_statement(writer, SERD_ANON_CONT, NULL, b0, p4, o4));

  assert(!serd_writer_end_anon(writer, b0));

  serd_node_free(s0);
  serd_node_free(p0);
  serd_node_free(b0);
  serd_node_free(p1);
  serd_node_free(b1);
  serd_node_free(p2);
  serd_node_free(o2);
  serd_node_free(p3);
  serd_node_free(p4);
  serd_node_free(o4);
  serd_node_free(nil);
  serd_writer_free(writer);
  serd_env_free(env);

  char* const out = serd_buffer_sink_finish(&buffer);

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
null_sink(const void* const buf,
          const size_t      size,
          const size_t      nmemb,
          void* const       stream)
{
  (void)buf;
  (void)stream;

  return size * nmemb;
}

static void
test_writer_cleanup(void)
{
  SerdStatus  st     = SERD_SUCCESS;
  SerdEnv*    env    = serd_env_new(serd_empty_string());
  SerdWriter* writer = serd_writer_new(SERD_TURTLE, 0U, env, null_sink, NULL);

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/p"));
  SerdNode* o = serd_new_blank(serd_string("start"));

  st = serd_writer_write_statement(writer, SERD_ANON_O_BEGIN, NULL, s, p, o);
  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 0U; !st && i < 8U; ++i) {
    char buf[12] = {0};
    snprintf(buf, sizeof(buf), "b%u", i);

    SerdNode* next_o = serd_new_blank(serd_string(buf));

    st = serd_writer_write_statement(
      writer, SERD_ANON_O_BEGIN, NULL, o, p, next_o);

    serd_node_free(o);
    o = next_o;
  }

  // Finish writing without terminating nodes
  assert(!(st = serd_writer_finish(writer)));
  assert(!(st = serd_writer_set_base_uri(writer, NULL)));

  // Set the base to an empty URI
  assert(!(st = serd_writer_set_base_uri(writer, NULL)));

  // Free (which could leak if the writer doesn't clean up the stack properly)
  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);
}

static void
test_strict_write(void)
{
  const char* const path = "serd_strict_write_test.ttl";
  FILE* const       fd   = fopen(path, "wb");
  assert(fd);

  SerdEnv* const    env    = serd_env_new(serd_empty_string());
  SerdWriter* const writer = serd_writer_new(
    SERD_TURTLE, (SerdWriterFlags)SERD_WRITE_STRICT, env, null_sink, fd);

  assert(writer);

  const uint8_t bad_str[] = {0xFF, 0x90, 'h', 'i', 0};

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/p"));

  SerdNode* bad_lit = serd_new_string(serd_string((const char*)bad_str));
  SerdNode* bad_uri = serd_new_uri(serd_string((const char*)bad_str));

  assert(serd_writer_write_statement(writer, 0, NULL, s, p, bad_lit) ==
         SERD_BAD_TEXT);

  assert(serd_writer_write_statement(writer, 0, NULL, s, p, bad_uri) ==
         SERD_BAD_TEXT);

  serd_node_free(bad_uri);
  serd_node_free(bad_lit);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);
  fclose(fd);
  remove(path);
}

// Produce a write error without setting errno
static size_t
error_sink(const void* const buf,
           const size_t      size,
           const size_t      len,
           void* const       stream)
{
  (void)buf;
  (void)size;
  (void)len;
  (void)stream;
  return 0U;
}

static void
test_write_error(void)
{
  SerdEnv* const env    = serd_env_new(serd_empty_string());
  SerdWriter*    writer = NULL;
  SerdStatus     st     = SERD_SUCCESS;

  SerdNode* u = serd_new_uri(serd_string("http://example.com/u"));

  writer =
    serd_writer_new(SERD_TURTLE, (SerdWriterFlags)0, env, error_sink, NULL);
  assert(writer);
  st = serd_writer_write_statement(writer, 0U, NULL, u, u, u);
  assert(st == SERD_BAD_WRITE);
  serd_writer_free(writer);

  serd_node_free(u);
  serd_env_free(env);
}

int
main(void)
{
  test_write_bad_prefix();
  test_write_long_literal();
  test_write_nested_anon();
  test_writer_cleanup();
  test_strict_write();
  test_write_error();

  return 0;
}
