// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
test_write_bad_event(void)
{
  SerdWorld*    world     = serd_world_new();
  SerdEnv*      env       = serd_env_new(serd_empty_string());
  SerdBuffer    buffer    = {NULL, 0};
  SerdByteSink* byte_sink = serd_byte_sink_new_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0U, env, byte_sink);
  assert(writer);

  const SerdEvent event = {(SerdEventType)42};
  assert(serd_sink_write_event(serd_writer_sink(writer), &event) ==
         SERD_ERR_BAD_ARG);

  assert(!serd_byte_sink_close(byte_sink));

  char* const out = (char*)buffer.buf;

  assert(!strcmp(out, ""));
  serd_free(out);

  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_long_literal(void)
{
  SerdWorld*    world     = serd_world_new();
  SerdEnv*      env       = serd_env_new(serd_empty_string());
  SerdBuffer    buffer    = {NULL, 0};
  SerdByteSink* byte_sink = serd_byte_sink_new_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0U, env, byte_sink);
  assert(writer);

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/p"));
  SerdNode* o = serd_new_literal(
    serd_string("hello \"\"\"world\"\"\"!"), SERD_IS_LONG, serd_empty_string());

  assert(serd_node_flags(o) & SERD_IS_LONG);
  assert(!serd_sink_write(serd_writer_sink(writer), 0, s, p, o, NULL));

  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);
  serd_env_free(env);

  char* out = serd_buffer_sink_finish(&buffer);

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

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
  SerdWorld*    world = serd_world_new();
  SerdEnv*      env   = serd_env_new(serd_empty_string());
  SerdByteSink* byte_sink =
    serd_byte_sink_new_function(null_sink, NULL, NULL, 1U);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0U, env, byte_sink);

  const SerdSink* sink = serd_writer_sink(writer);

  SerdNode* const s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* const p = serd_new_uri(serd_string("http://example.org/p"));

  SerdNode*  o  = serd_new_token(SERD_BLANK, serd_string("blank"));
  SerdStatus st = serd_sink_write(sink, SERD_ANON_O, s, p, o, NULL);
  assert(!st);

  // Repeatedly write nested anonymous objects until the writer stack overflows
  for (unsigned i = 0U; i < 512U; ++i) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "b%u", i);

    SerdNode* next_o = serd_new_token(SERD_BLANK, serd_string(buf));

    st = serd_sink_write(sink, SERD_ANON_O, o, p, next_o, NULL);

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
  serd_byte_sink_free(byte_sink);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_strict_write(void)
{
  SerdWorld*  world = serd_world_new();
  const char* path  = "serd_strict_write_test.ttl";
  FILE*       fd    = fopen(path, "wb");
  assert(fd);

  SerdEnv* env = serd_env_new(serd_empty_string());

  SerdByteSink* byte_sink = serd_byte_sink_new_function(
    (SerdWriteFunc)fwrite, (SerdStreamCloseFunc)fclose, fd, 1);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, byte_sink);
  assert(writer);

  const SerdSink*      sink      = serd_writer_sink(writer);
  const uint8_t        bad_str[] = {0xFF, 0x90, 'h', 'i', 0};
  const SerdStringView bad_view  = {(const char*)bad_str, 4};

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/s"));

  SerdNode* bad_lit = serd_new_string(bad_view);
  SerdNode* bad_uri = serd_new_uri(bad_view);

  assert(serd_sink_write(sink, 0, s, p, bad_lit, 0) == SERD_ERR_BAD_TEXT);
  assert(serd_sink_write(sink, 0, s, p, bad_uri, 0) == SERD_ERR_BAD_TEXT);

  serd_node_free(bad_uri);
  serd_node_free(bad_lit);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);
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
    return 0U;
  }

  return size * nmemb;
}

static void
test_write_error(void)
{
  SerdWorld* world = serd_world_new();
  SerdEnv*   env   = serd_env_new(serd_empty_string());

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/p"));
  SerdNode* o = serd_new_uri(serd_string("http://example.org/o"));

  // Test with setting errno

  SerdByteSink* byte_sink =
    serd_byte_sink_new_function(faulty_sink, NULL, NULL, 1);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0U, env, byte_sink);
  assert(writer);

  SerdStatus st = serd_sink_write(serd_writer_sink(writer), 0U, s, p, o, NULL);
  assert(st == SERD_ERR_BAD_WRITE);

  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);

  // Test without setting errno
  byte_sink = serd_byte_sink_new_function(faulty_sink, NULL, world, 1);
  writer    = serd_writer_new(world, SERD_TURTLE, 0U, env, byte_sink);

  assert(writer);

  assert(serd_sink_write(serd_writer_sink(writer), 0U, s, p, o, NULL) ==
         SERD_ERR_BAD_WRITE);

  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);

  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_empty_syntax(void)
{
  SerdWorld* world = serd_world_new();
  SerdEnv*   env   = serd_env_new(serd_empty_string());

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/p"));
  SerdNode* o = serd_new_uri(serd_string("http://example.org/o"));

  SerdBuffer    buffer    = {NULL, 0};
  SerdByteSink* byte_sink = serd_byte_sink_new_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_SYNTAX_EMPTY, 0U, env, byte_sink);

  assert(writer);

  assert(!serd_sink_write(serd_writer_sink(writer), 0U, s, p, o, NULL));
  assert(!serd_byte_sink_close(byte_sink));

  char* const out = (char*)buffer.buf;

  assert(strlen(out) == 0);
  serd_free(out);

  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);
  serd_node_free(o);
  serd_node_free(p);
  serd_node_free(s);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_bad_uri(void)
{
  SerdWorld* world = serd_world_new();
  SerdEnv*   env   = serd_env_new(serd_empty_string());

  SerdNode* s   = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p   = serd_new_uri(serd_string("http://example.org/p"));
  SerdNode* rel = serd_new_uri(serd_string("rel"));

  SerdBuffer    buffer    = {NULL, 0};
  SerdByteSink* byte_sink = serd_byte_sink_new_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_NTRIPLES, 0U, env, byte_sink);

  assert(writer);

  const SerdStatus st =
    serd_sink_write(serd_writer_sink(writer), 0U, s, p, rel, NULL);

  assert(st);
  assert(st == SERD_ERR_BAD_ARG);

  serd_byte_sink_close(byte_sink);
  serd_free(buffer.buf);
  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);
  serd_node_free(rel);
  serd_node_free(p);
  serd_node_free(s);
  serd_env_free(env);
  serd_world_free(world);
}

static void
check_pname_escape(const char* const lname, const char* const expected)
{
  SerdWorld*    world     = serd_world_new();
  SerdEnv*      env       = serd_env_new(serd_empty_string());
  SerdBuffer    buffer    = {NULL, 0};
  SerdByteSink* byte_sink = serd_byte_sink_new_buffer(&buffer);

  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0U, env, byte_sink);
  assert(writer);

  static const char* const prefix     = "http://example.org/";
  const size_t             prefix_len = strlen(prefix);

  serd_env_set_prefix(env, serd_string("eg"), serd_string(prefix));

  SerdNode* s = serd_new_uri(serd_string("http://example.org/s"));
  SerdNode* p = serd_new_uri(serd_string("http://example.org/p"));

  char* const uri = (char*)calloc(1, prefix_len + strlen(lname) + 1);
  memcpy(uri, prefix, prefix_len + 1);
  memcpy(uri + prefix_len, lname, strlen(lname) + 1);

  SerdNode* node = serd_new_uri(serd_string(uri));
  assert(!serd_sink_write(serd_writer_sink(writer), 0, s, p, node, NULL));
  serd_node_free(node);

  free(uri);
  serd_node_free(p);
  serd_node_free(s);
  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);
  serd_env_free(env);

  char* out = serd_buffer_sink_finish(&buffer);
  assert(!strcmp((char*)out, expected));
  serd_free(out);

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
  static const char first_escape[] = {(char)0xC3U, (char)0xB7U, 'y', 'z', 0};
  static const char mid_escape[]   = {'w', (char)0xC3U, (char)0xB7U, 'z', 0};
  static const char last_escape[]  = {'w', 'x', (char)0xC3U, (char)0xB7U, 0};

  check_pname_escape((const char*)first_escape, "eg:s\n\teg:p eg:%C3%B7yz .\n");
  check_pname_escape((const char*)mid_escape, "eg:s\n\teg:p eg:w%C3%B7z .\n");
  check_pname_escape((const char*)last_escape, "eg:s\n\teg:p eg:wx%C3%B7 .\n");
}

int
main(void)
{
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
