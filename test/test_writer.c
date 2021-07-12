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
#include <stdio.h>
#include <string.h>

static void
test_write_bad_prefix(void)
{
  SerdWorld*  world  = serd_world_new();
  SerdEnv*    env    = serd_env_new(SERD_EMPTY_STRING());
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0u, env, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* name = serd_new_string(SERD_STRING("eg"));
  SerdNode* uri  = serd_new_uri(SERD_STRING("rel"));

  assert(serd_sink_write_prefix(serd_writer_sink(writer), name, uri) ==
         SERD_ERR_BAD_ARG);

  char* const out = serd_buffer_sink_finish(&buffer);

  assert(!strcmp(out, ""));
  serd_free(out);

  serd_node_free(uri);
  serd_node_free(name);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_long_literal(void)
{
  SerdWorld*  world  = serd_world_new();
  SerdEnv*    env    = serd_env_new(SERD_EMPTY_STRING());
  SerdBuffer  buffer = {NULL, 0};
  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0u, env, serd_buffer_sink, &buffer);

  assert(writer);

  SerdNode* s = serd_new_uri(SERD_STRING("http://example.org/s"));
  SerdNode* p = serd_new_uri(SERD_STRING("http://example.org/p"));
  SerdNode* o = serd_new_string(SERD_STRING("hello \"\"\"world\"\"\"!"));

  assert(!serd_sink_write(serd_writer_sink(writer), 0, s, p, o, NULL));

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

  serd_world_free(world);
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
test_writer_stack_overflow(void)
{
  SerdWorld* world = serd_world_new();
  SerdEnv*   env   = serd_env_new(SERD_EMPTY_STRING());

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0u, env, null_sink, NULL);

  const SerdSink* sink = serd_writer_sink(writer);

  SerdNode* const s = serd_new_uri(SERD_STRING("http://example.org/s"));
  SerdNode* const p = serd_new_uri(SERD_STRING("http://example.org/p"));

  SerdNode*  o  = serd_new_blank(SERD_STRING("http://example.org/o"));
  SerdStatus st = serd_sink_write(sink, SERD_ANON_O_BEGIN, s, p, o, NULL);
  assert(!st);

  // Repeatedly write nested anonymous objects until the writer stack overflows
  for (unsigned i = 0u; i < 512u; ++i) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "b%u", i);

    SerdNode* next_o = serd_new_blank(SERD_STRING(buf));

    st = serd_sink_write(sink, SERD_ANON_O_BEGIN, o, p, next_o, NULL);

    serd_node_free(o);
    o = next_o;

    if (st) {
      assert(st == SERD_ERR_OVERFLOW);
      break;
    }
  }

  assert(st == SERD_ERR_OVERFLOW);

  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

int
main(void)
{
  test_write_bad_prefix();
  test_write_long_literal();
  test_writer_stack_overflow();

  return 0;
}
