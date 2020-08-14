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

  // Free (which could leak if the writer doesn't clean up the stack properly)
  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);
}

int
main(void)
{
  test_write_long_literal();
  test_writer_cleanup();

  return 0;
}
