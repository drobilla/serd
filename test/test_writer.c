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
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
test_writer_new(void)
{
  SerdWorld*       world  = serd_world_new();
  SerdEnv*         env    = serd_env_new(SERD_EMPTY_STRING());
  SerdBuffer       buffer = {NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  assert(!serd_writer_new(world, SERD_TURTLE, 0u, env, &output, 0));
  assert(!serd_writer_new(world, SERD_TURTLE, 0u, env, &output, SIZE_MAX));
}

static void
test_write_bad_event(void)
{
  SerdWorld*       world  = serd_world_new();
  SerdEnv*         env    = serd_env_new(SERD_EMPTY_STRING());
  SerdBuffer       buffer = {NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0u, env, &output, 1);
  assert(writer);

  const SerdEvent event = {(SerdEventType)42};
  assert(serd_sink_write_event(serd_writer_sink(writer), &event) ==
         SERD_ERR_BAD_ARG);

  assert(!serd_close_output(&output));

  char* const out = (char*)buffer.buf;

  assert(!strcmp(out, ""));
  serd_free(out);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_long_literal(void)
{
  SerdWorld*       world  = serd_world_new();
  SerdNodes*       nodes  = serd_world_nodes(world);
  SerdEnv*         env    = serd_env_new(SERD_EMPTY_STRING());
  SerdBuffer       buffer = {NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0u, env, &output, 1);
  assert(writer);

  const SerdNode* s =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* p =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/p"));

  const SerdNode* o =
    serd_nodes_literal(nodes,
                       SERD_STRING("hello \"\"\"world\"\"\"!"),
                       SERD_IS_LONG,
                       SERD_EMPTY_STRING());

  assert(serd_node_flags(o) & SERD_IS_LONG);
  assert(!serd_sink_write(serd_writer_sink(writer), 0, s, p, o, NULL));

  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);
  serd_buffer_close(&buffer);

  char* out = (char*)buffer.buf;

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

  assert(!strcmp(out, expected));
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
  SerdWorld*       world  = serd_world_new();
  SerdNodes*       nodes  = serd_world_nodes(world);
  SerdEnv*         env    = serd_env_new(SERD_EMPTY_STRING());
  SerdOutputStream output = serd_open_output_stream(null_sink, NULL, NULL);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0u, env, &output, 1);

  const SerdSink* sink = serd_writer_sink(writer);

  const SerdNode* const s =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* const p =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/p"));

  const SerdNode* o = serd_nodes_blank(nodes, SERD_STRING("blank"));

  SerdStatus st = serd_sink_write(sink, SERD_ANON_O, s, p, o, NULL);
  assert(!st);

  // Repeatedly write nested anonymous objects until the writer stack overflows
  for (unsigned i = 0u; i < 512u; ++i) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "b%u", i);

    const SerdNode* next_o = serd_nodes_blank(nodes, SERD_STRING(buf));

    st = serd_sink_write(sink, SERD_ANON_O, o, p, next_o, NULL);

    o = next_o;

    if (st) {
      assert(st == SERD_ERR_OVERFLOW);
      break;
    }
  }

  assert(st == SERD_ERR_OVERFLOW);

  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_strict_write(void)
{
  static const char* path = "serd_strict_write_test.ttl";

  SerdWorld* world = serd_world_new();
  SerdNodes* nodes = serd_world_nodes(world);
  SerdEnv*   env   = serd_env_new(SERD_EMPTY_STRING());

  SerdOutputStream output = serd_open_output_file(path);
  assert(output.stream);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, &output, 1);
  assert(writer);

  const SerdSink*      sink      = serd_writer_sink(writer);
  const uint8_t        bad_str[] = {0xFF, 0x90, 'h', 'i', 0};
  const SerdStringView bad_view  = {(const char*)bad_str, 4};

  const SerdNode* s =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* p =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* bad_lit = serd_nodes_string(nodes, bad_view);
  const SerdNode* bad_uri = serd_nodes_uri(nodes, bad_view);

  assert(serd_sink_write(sink, 0, s, p, bad_lit, 0) == SERD_ERR_BAD_TEXT);
  assert(serd_sink_write(sink, 0, s, p, bad_uri, 0) == SERD_ERR_BAD_TEXT);

  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);
  serd_world_free(world);
}

static size_t
faulty_sink(const void* const buf,
            const size_t      size,
            const size_t      nmemb,
            void* const       stream)
{
  (void)buf;
  (void)size;
  (void)nmemb;

  if (nmemb > 1) {
    errno = stream ? ERANGE : 0;
    return 0u;
  }

  return size * nmemb;
}

static void
test_write_error(void)
{
  SerdWorld* world = serd_world_new();
  SerdNodes* nodes = serd_world_nodes(world);
  SerdEnv*   env   = serd_env_new(SERD_EMPTY_STRING());

  const SerdNode* s =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* p =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/p"));

  const SerdNode* o =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/o"));

  // Test with setting errno

  SerdOutputStream output = serd_open_output_stream(faulty_sink, NULL, NULL);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0u, env, &output, 1);
  assert(writer);

  SerdStatus st = serd_sink_write(serd_writer_sink(writer), 0u, s, p, o, NULL);
  assert(st == SERD_ERR_BAD_WRITE);

  serd_writer_free(writer);
  serd_close_output(&output);

  // Test without setting errno
  output = serd_open_output_stream(faulty_sink, NULL, world);
  writer = serd_writer_new(world, SERD_TURTLE, 0u, env, &output, 1);

  assert(writer);

  assert(serd_sink_write(serd_writer_sink(writer), 0u, s, p, o, NULL) ==
         SERD_ERR_BAD_WRITE);

  serd_writer_free(writer);
  serd_close_output(&output);

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_empty_syntax(void)
{
  SerdWorld* world = serd_world_new();
  SerdNodes* nodes = serd_world_nodes(world);
  SerdEnv*   env   = serd_env_new(SERD_EMPTY_STRING());

  const SerdNode* s =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* p =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/p"));

  const SerdNode* o =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/o"));

  SerdBuffer       buffer = {NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_SYNTAX_EMPTY, 0u, env, &output, 1);

  assert(writer);

  assert(!serd_sink_write(serd_writer_sink(writer), 0u, s, p, o, NULL));
  assert(!serd_close_output(&output));

  char* const out = (char*)buffer.buf;

  assert(strlen(out) == 0);
  serd_free(out);

  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_bad_uri(void)
{
  SerdWorld* world = serd_world_new();
  SerdNodes* nodes = serd_world_nodes(world);
  SerdEnv*   env   = serd_env_new(SERD_EMPTY_STRING());

  const SerdNode* s =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* p =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/p"));

  const SerdNode* rel = serd_nodes_uri(nodes, SERD_STRING("rel"));

  SerdBuffer       buffer = {NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_NTRIPLES, 0u, env, &output, 1);

  assert(writer);

  const SerdStatus st =
    serd_sink_write(serd_writer_sink(writer), 0u, s, p, rel, NULL);

  assert(st);
  assert(st == SERD_ERR_BAD_ARG);

  serd_close_output(&output);
  serd_free(buffer.buf);
  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);
  serd_world_free(world);
}

static void
check_pname_escape(const char* const lname, const char* const expected)
{
  SerdWorld*       world  = serd_world_new();
  SerdNodes*       nodes  = serd_world_nodes(world);
  SerdEnv*         env    = serd_env_new(SERD_EMPTY_STRING());
  SerdBuffer       buffer = {NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0u, env, &output, 1);
  assert(writer);

  static const char* const prefix     = "http://example.org/";
  const size_t             prefix_len = strlen(prefix);

  serd_env_set_prefix(env, SERD_STRING("eg"), SERD_STRING(prefix));

  const SerdNode* s =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/s"));

  const SerdNode* p =
    serd_nodes_uri(nodes, SERD_STRING("http://example.org/p"));

  char* const uri = (char*)calloc(1, prefix_len + strlen(lname) + 1);
  memcpy(uri, prefix, prefix_len + 1);
  memcpy(uri + prefix_len, lname, strlen(lname) + 1);

  const SerdNode* node = serd_nodes_uri(nodes, SERD_STRING(uri));

  assert(!serd_sink_write(serd_writer_sink(writer), 0, s, p, node, NULL));

  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);
  serd_buffer_close(&buffer);

  char* out = (char*)buffer.buf;

  assert(!strcmp(out, expected));
  serd_free(out);

  free(uri);
  serd_world_free(world);
}

static void
test_write_pname_escapes(void)
{
  // Check that '.' is escaped only at the start and end
  check_pname_escape(".xyz", "eg:s\n\teg:p eg:\\.xyz .\n");
  check_pname_escape("w.yz", "eg:s\n\teg:p eg:w.yz .\n");
  check_pname_escape("wx.z", "eg:s\n\teg:p eg:wx.z .\n");
  check_pname_escape("wxy.", "eg:s\n\teg:p eg:wxy\\. .\n");

  // Check that ':' is not escaped anywhere
  check_pname_escape(":xyz", "eg:s\n\teg:p eg::xyz .\n");
  check_pname_escape("w:yz", "eg:s\n\teg:p eg:w:yz .\n");
  check_pname_escape("wx:z", "eg:s\n\teg:p eg:wx:z .\n");
  check_pname_escape("wxy:", "eg:s\n\teg:p eg:wxy: .\n");

  // Check that special characters like '~' are escaped everywhere
  check_pname_escape("~xyz", "eg:s\n\teg:p eg:\\~xyz .\n");
  check_pname_escape("w~yz", "eg:s\n\teg:p eg:w\\~yz .\n");
  check_pname_escape("wx~z", "eg:s\n\teg:p eg:wx\\~z .\n");
  check_pname_escape("wxy~", "eg:s\n\teg:p eg:wxy\\~ .\n");

  // Check that out of range multi-byte characters are escaped everywhere
  static const char first_escape[] = {(char)0xC3u, (char)0xB7u, 'y', 'z', 0};
  static const char mid_escape[]   = {'w', (char)0xC3u, (char)0xB7u, 'z', 0};
  static const char last_escape[]  = {'w', 'x', (char)0xC3u, (char)0xB7u, 0};

  check_pname_escape((const char*)first_escape, "eg:s\n\teg:p eg:%C3%B7yz .\n");
  check_pname_escape((const char*)mid_escape, "eg:s\n\teg:p eg:w%C3%B7z .\n");
  check_pname_escape((const char*)last_escape, "eg:s\n\teg:p eg:wx%C3%B7 .\n");
}

int
main(void)
{
  test_writer_new();
  test_write_bad_event();
  test_write_long_literal();
  test_writer_stack_overflow();
  test_strict_write();
  test_write_error();
  test_write_empty_syntax();
  test_write_bad_uri();
  test_write_pname_escapes();

  return 0;
}
