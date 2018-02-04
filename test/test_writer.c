// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/memory.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NS_EG "http://example.org/"

static void
test_write_long_literal(void)
{
  SerdWorld*  world  = serd_world_new();
  SerdEnv*    env    = serd_env_new(NULL);
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer = serd_writer_new(
    world, SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* s = serd_new_string(SERD_URI, NS_EG "s");
  SerdNode* p = serd_new_string(SERD_URI, NS_EG "p");
  SerdNode* o = serd_new_string(SERD_LITERAL, "hello \"\"\"world\"\"\"!");

  assert(!serd_writer_write_statement(writer, 0, NULL, s, p, o, NULL, NULL));

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

  serd_world_free(world);
}

static void
test_write_nested_anon(void)
{
  SerdWorld*  world  = serd_world_new();
  SerdEnv*    env    = serd_env_new(NULL);
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer = serd_writer_new(
    world, SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* s0 = serd_new_string(SERD_URI, NS_EG "s0");
  SerdNode* p0 = serd_new_string(SERD_URI, NS_EG "p0");
  SerdNode* b0 = serd_new_string(SERD_BLANK, "b0");
  SerdNode* p1 = serd_new_string(SERD_URI, NS_EG "p1");
  SerdNode* b1 = serd_new_string(SERD_BLANK, "b1");
  SerdNode* p2 = serd_new_string(SERD_URI, NS_EG "p2");
  SerdNode* o2 = serd_new_string(SERD_URI, NS_EG "o2");
  SerdNode* p3 = serd_new_string(SERD_URI, NS_EG "p3");
  SerdNode* p4 = serd_new_string(SERD_URI, NS_EG "p4");
  SerdNode* o4 = serd_new_string(SERD_URI, NS_EG "o4");
  SerdNode* nil =
    serd_new_string(SERD_URI, "http://www.w3.org/1999/02/22-rdf-syntax-ns#nil");

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, s0, p0, b0, NULL, NULL));

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, b0, p1, b1, NULL, NULL));

  assert(
    !serd_writer_write_statement(writer, 0U, NULL, b1, p2, o2, NULL, NULL));

  assert(!serd_writer_write_statement(
    writer, SERD_LIST_O, NULL, b1, p3, nil, NULL, NULL));

  assert(!serd_writer_end_anon(writer, b1));
  assert(
    !serd_writer_write_statement(writer, 0U, NULL, b0, p4, o4, NULL, NULL));

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
  serd_world_free(world);
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
  SerdStatus        st    = SERD_SUCCESS;
  SerdWorld* const  world = serd_world_new();
  SerdEnv* const    env   = serd_env_new(NULL);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, NULL, null_sink, NULL);

  SerdNode* const s = serd_new_string(SERD_URI, NS_EG "s");
  SerdNode* const p = serd_new_string(SERD_URI, NS_EG "p");
  SerdNode*       o = serd_new_string(SERD_BLANK, "b0");

  st =
    serd_writer_write_statement(writer, SERD_ANON_O, NULL, s, p, o, NULL, NULL);

  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 1U; !st && i < 9U; ++i) {
    char buf[12] = {'\0'};
    snprintf(buf, sizeof(buf), "b%u", i);

    SerdNode* next_o = serd_new_string(SERD_BLANK, buf);

    st = serd_writer_write_statement(
      writer, SERD_ANON_O, NULL, o, p, next_o, NULL, NULL);

    assert(!st);

    serd_node_free(o);
    o = next_o;
  }

  // Finish writing without terminating nodes
  assert(!(st = serd_writer_finish(writer)));

  // Set the base to an empty URI
  assert(!(st = serd_writer_set_base_uri(writer, NULL)));

  // Free (which could leak if the writer doesn't clean up the stack properly)
  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_bad_anon_stack(void)
{
  SerdStatus        st    = SERD_SUCCESS;
  SerdWorld* const  world = serd_world_new();
  SerdEnv* const    env   = serd_env_new(NULL);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, NULL, null_sink, NULL);

  SerdNode* s  = serd_new_string(SERD_URI, NS_EG "s");
  SerdNode* p  = serd_new_string(SERD_URI, NS_EG "p");
  SerdNode* b0 = serd_new_string(SERD_BLANK, "b0");
  SerdNode* b1 = serd_new_string(SERD_BLANK, "b1");
  SerdNode* b2 = serd_new_string(SERD_BLANK, "b2");

  assert(!(st = serd_writer_write_statement(
             writer, SERD_ANON_O, NULL, s, p, b0, NULL, NULL)));

  // (missing call to end the anonymous node here)

  st = serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, b1, p, b2, NULL, NULL);

  assert(st == SERD_BAD_ARG);

  assert(!(st = serd_writer_finish(writer)));

  serd_node_free(b2);
  serd_node_free(b1);
  serd_node_free(b0);
  serd_node_free(p);
  serd_node_free(s);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_strict_write(void)
{
  const char* const path = "serd_strict_write_test.ttl";
  FILE* const       fd   = fopen(path, "wb");
  assert(fd);

  SerdWorld* const  world = serd_world_new();
  SerdEnv* const    env   = serd_env_new(NULL);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, NULL, null_sink, fd);

  assert(writer);

  const uint8_t bad_str[] = {0xFF, 0x90, 'h', 'i', 0};

  SerdNode* s = serd_new_string(SERD_URI, NS_EG "s");
  SerdNode* p = serd_new_string(SERD_URI, NS_EG "p");

  SerdNode* bad_lit = serd_new_string(SERD_LITERAL, (const char*)bad_str);
  SerdNode* bad_uri = serd_new_string(SERD_URI, (const char*)bad_str);

  assert(serd_writer_write_statement(
           writer, 0, NULL, s, p, bad_lit, NULL, NULL) == SERD_BAD_TEXT);

  assert(serd_writer_write_statement(
           writer, 0, NULL, s, p, bad_uri, NULL, NULL) == SERD_BAD_TEXT);

  serd_node_free(bad_uri);
  serd_node_free(bad_lit);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
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
  SerdWorld* const world = serd_world_new();
  SerdEnv* const   env   = serd_env_new(NULL);
  SerdStatus       st    = SERD_SUCCESS;

  SerdNode* u = serd_new_string(SERD_URI, NS_EG "u");

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, NULL, error_sink, NULL);
  assert(writer);

  st = serd_writer_write_statement(writer, 0U, NULL, u, u, u, NULL, NULL);
  assert(st == SERD_BAD_WRITE);
  serd_writer_free(writer);

  serd_node_free(u);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_empty_syntax(void)
{
  SerdWorld* const world = serd_world_new();
  SerdEnv*         env   = serd_env_new(NULL);

  SerdNode* s = serd_new_uri(NS_EG "s");
  SerdNode* p = serd_new_uri(NS_EG "p");
  SerdNode* o = serd_new_string(SERD_CURIE, "eg:o");

  SerdBuffer buffer = {NULL, 0};

  SerdWriter* const writer = serd_writer_new(
    world, SERD_SYNTAX_EMPTY, 0U, env, NULL, serd_buffer_sink, &buffer);

  assert(writer);

  assert(!serd_writer_write_statement(writer, 0U, NULL, s, p, o, NULL, NULL));

  char* const out = serd_buffer_sink_finish(&buffer);

  assert(strlen(out) == 0);
  serd_free(out);

  serd_writer_free(writer);
  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_env_free(env);
  serd_world_free(world);
}

int
main(void)
{
  test_write_long_literal();
  test_write_nested_anon();
  test_writer_cleanup();
  test_write_bad_anon_stack();
  test_strict_write();
  test_write_error();
  test_write_empty_syntax();

  return 0;
}

#undef NS_EG
