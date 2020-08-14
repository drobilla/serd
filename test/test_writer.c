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
#include "serd/writer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
test_write_long_literal(void)
{
  SerdEnv*    env    = serd_env_new(NULL);
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer =
    serd_writer_new(SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* s = serd_new_string(SERD_URI, "http://example.org/s");
  SerdNode* p = serd_new_string(SERD_URI, "http://example.org/p");
  SerdNode* o = serd_new_string(SERD_LITERAL, "hello \"\"\"world\"\"\"!");

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
    serd_writer_new(SERD_TURTLE, 0U, env, NULL, null_sink, NULL);

  SerdNode* s = serd_new_string(SERD_URI, "http://example.org/s");
  SerdNode* p = serd_new_string(SERD_URI, "http://example.org/p");
  SerdNode* o = serd_new_string(SERD_BLANK, "http://example.org/o");

  st = serd_writer_write_statement(writer, SERD_ANON_O_BEGIN, NULL, s, p, o);
  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 0U; !st && i < 8U; ++i) {
    char buf[12] = {0};
    snprintf(buf, sizeof(buf), "b%u", i);

    SerdNode* next_o = serd_new_string(SERD_BLANK, buf);

    st = serd_writer_write_statement(
      writer, SERD_ANON_O_BEGIN, NULL, o, p, next_o);

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
}

static void
test_strict_write(void)
{
  const char* const path = "serd_strict_write_test.ttl";
  FILE* const       fd   = fopen(path, "wb");
  assert(fd);

  SerdEnv* const    env    = serd_env_new(NULL);
  SerdWriter* const writer = serd_writer_new(
    SERD_TURTLE, (SerdWriterFlags)SERD_WRITE_STRICT, env, NULL, null_sink, fd);

  assert(writer);

  const uint8_t bad_str[] = {0xFF, 0x90, 'h', 'i', 0};

  SerdNode* s = serd_new_string(SERD_URI, "http://example.org/s");
  SerdNode* p = serd_new_string(SERD_URI, "http://example.org/p");

  SerdNode* bad_lit = serd_new_string(SERD_LITERAL, (const char*)bad_str);
  SerdNode* bad_uri = serd_new_string(SERD_URI, (const char*)bad_str);

  assert(serd_writer_write_statement(writer, 0, NULL, s, p, bad_lit) ==
         SERD_ERR_BAD_TEXT);

  assert(serd_writer_write_statement(writer, 0, NULL, s, p, bad_uri) ==
         SERD_ERR_BAD_TEXT);

  serd_writer_free(writer);
  serd_env_free(env);
  fclose(fd);
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

  SerdNode* u = serd_new_string(SERD_URI, "http://example.com/u");

  writer = serd_writer_new(
    SERD_TURTLE, (SerdWriterFlags)0, env, NULL, error_sink, NULL);
  assert(writer);
  st = serd_writer_write_statement(writer, 0U, NULL, u, u, u);
  assert(st == SERD_ERR_BAD_WRITE);
  serd_writer_free(writer);

  serd_env_free(env);
}

int
main(void)
{
  test_write_long_literal();
  test_writer_cleanup();
  test_strict_write();
  test_write_error();

  return 0;
}
