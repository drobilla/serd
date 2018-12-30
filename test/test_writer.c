/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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
#include <string.h>

static void
test_write_long_literal(void)
{
  SerdEnv*    env    = serd_env_new(NULL);
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer =
    serd_writer_new(SERD_TURTLE, 0u, env, NULL, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode s = serd_node_from_string(SERD_URI, "http://example.org/s");
  SerdNode p = serd_node_from_string(SERD_URI, "http://example.org/p");
  SerdNode o = serd_node_from_string(SERD_LITERAL, "hello \"\"\"world\"\"\"!");

  assert(!serd_writer_write_statement(writer, 0, NULL, &s, &p, &o, NULL, NULL));

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
