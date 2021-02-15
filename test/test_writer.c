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

  uint8_t* out = serd_chunk_sink_finish(&chunk);

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n\n";

  assert(!strcmp((char*)out, expected));
  serd_free(out);
}

int
main(void)
{
  const char* const path = "serd_test.ttl";

  test_writer(path);
  test_write_long_literal();

  printf("Success\n");
  return 0;
}
