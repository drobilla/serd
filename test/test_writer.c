// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/memory.h"
#include "serd/node.h"
#include "serd/syntax.h"
#include "serd/writer.h"

#include <assert.h>
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

  assert(!serd_writer_write_statement(writer, 0, NULL, s, p, o, NULL, NULL));

  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);

  char* out = serd_buffer_sink_finish(&buffer);

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n\n";

  assert(!strcmp((char*)out, expected));
  serd_free(out);
}

int
main(void)
{
  test_write_long_literal();

  return 0;
}
