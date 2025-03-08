// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/buffer.h>
#include <serd/env.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/statement_flags.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_EG "http://example.org/"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

static size_t
null_sink(const void* const buf, const size_t len, void* const stream)
{
  (void)buf;
  (void)stream;

  return len;
}

static SerdStatus
write_statement(SerdWriter* const        writer,
                const SerdStatementFlags flags,
                const SerdTokenView      subject,
                const SerdTokenView      predicate,
                const SerdObjectView     object)
{
  return serd_writer_write_statement(
    writer, flags, serd_no_token(), subject, predicate, object);
}

static void
test_new_failed_alloc(void)
{
  static const SerdWriterFlags flags = (SerdWriterFlags)SERD_WRITE_BULK;

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  SerdEnv* const   env   = serd_env_new(&allocator.base, zix_empty_string());

  // Successfully allocate a writer to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, flags, env, null_sink, NULL);
  assert(writer);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_writer_new(world, SERD_TURTLE, flags, env, null_sink, NULL));
  }

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_long_literal(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, serd_buffer_sink, &buffer);
  assert(writer);

  const SerdTokenView  s = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView  p = {SERD_URI, zix_string(NS_EG "p")};
  const SerdObjectView o = {SERD_LITERAL,
                            zix_string("hello \"\"\"world\"\"\"!"),
                            SERD_HAS_QUOTE,
                            serd_no_token()};

  assert(!write_statement(writer, 0U, s, p, o));
  assert(!serd_writer_finish(writer));

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

  const char* const out = serd_buffer_sink_finish(&buffer);
  assert(expect_string(out, expected));
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_nested_anon(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, serd_buffer_sink, &buffer);
  assert(writer);

  const SerdTokenView s0  = {SERD_URI, zix_string(NS_EG "s0")};
  const SerdTokenView p0  = {SERD_URI, zix_string(NS_EG "p0")};
  const SerdTokenView b0  = {SERD_BLANK, zix_string(NS_EG "b0")};
  const SerdTokenView p1  = {SERD_URI, zix_string(NS_EG "p1")};
  const SerdTokenView b1  = {SERD_BLANK, zix_string(NS_EG "b1")};
  const SerdTokenView p2  = {SERD_URI, zix_string(NS_EG "p2")};
  const SerdTokenView o2  = {SERD_URI, zix_string(NS_EG "o2")};
  const SerdTokenView p3  = {SERD_URI, zix_string(NS_EG "p3")};
  const SerdTokenView p4  = {SERD_URI, zix_string(NS_EG "p4")};
  const SerdTokenView o4  = {SERD_URI, zix_string(NS_EG "o4")};
  const SerdTokenView nil = {SERD_URI, zix_string(NS_RDF "nil")};

  assert(
    !write_statement(writer, SERD_ANON_O, s0, p0, serd_token_object_view(b0)));
  assert(
    !write_statement(writer, SERD_ANON_O, b0, p1, serd_token_object_view(b1)));
  assert(!write_statement(writer, 0U, b1, p2, serd_token_object_view(o2)));
  assert(!write_statement(writer, 0U, b1, p3, serd_token_object_view(nil)));
  assert(!serd_writer_end_anon(writer, b1.string));
  assert(!write_statement(writer, 0U, b0, p4, serd_token_object_view(o4)));
  assert(!serd_writer_end_anon(writer, b0.string));
  assert(!serd_writer_finish(writer));

  static const char* const expected =
    "<http://example.org/s0>\n"
    "\t<http://example.org/p0> [\n"
    "\t\t<http://example.org/p1> [\n"
    "\t\t\t<http://example.org/p2> <http://example.org/o2> ;\n"
    "\t\t\t<http://example.org/p3> ()\n"
    "\t\t] ;\n"
    "\t\t<http://example.org/p4> <http://example.org/o4>\n"
    "\t] .\n";

  const char* const out = serd_buffer_sink_finish(&buffer);
  assert(expect_string(out, expected));
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_writer_cleanup(void)
{
  SerdStatus       st    = SERD_SUCCESS;
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, null_sink, NULL);
  assert(writer);

  char buf[12] = {'b', '0', '\0'};

  SerdTokenView s = {SERD_URI, zix_string(NS_EG "s")};
  SerdTokenView p = {SERD_URI, zix_string(NS_EG "p")};
  SerdTokenView o = {SERD_BLANK, zix_string(buf)};

  st = write_statement(writer, SERD_ANON_O, s, p, serd_token_object_view(o));
  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 1U; !st && i < 9U; ++i) {
    char temp[12] = {'\0'};
    snprintf(temp, sizeof(temp), "b%u", i);

    const SerdTokenView next_o = {SERD_BLANK, zix_string(temp)};

    st = write_statement(
      writer, SERD_ANON_O, o, p, serd_token_object_view(next_o));
    assert(!st);

    memcpy(buf, temp, sizeof(buf));
    o.string = zix_string(buf);
  }

  // Finish writing without terminating nodes
  st = serd_writer_finish(writer);
  assert(!st);

  // Set the base to an empty URI
  st = serd_writer_set_base_uri(writer, zix_string(""));
  assert(!st);

  // Free (which could leak if the writer doesn't clean up the stack properly)
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_bad_anon_stack(void)
{
  SerdStatus       st    = SERD_SUCCESS;
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, null_sink, NULL);
  assert(writer);

  SerdTokenView  s  = {SERD_URI, zix_string(NS_EG "s")};
  SerdTokenView  p  = {SERD_URI, zix_string(NS_EG "p")};
  SerdObjectView b0 = {SERD_BLANK, zix_string(NS_EG "b0"), 0U, serd_no_token()};
  SerdTokenView  b1 = {SERD_BLANK, zix_string(NS_EG "b1")};
  SerdObjectView b2 = {SERD_BLANK, zix_string(NS_EG "b2"), 0U, serd_no_token()};

  st = write_statement(writer, SERD_ANON_O, s, p, b0);
  assert(!st);

  // (missing call to end the anonymous node here)

  st = write_statement(writer, SERD_ANON_O, b1, p, b2);
  assert(st == SERD_BAD_ARG);

  st = serd_writer_finish(writer);
  assert(!st);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
check_strict_write(const SerdWriterFlags flags)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, flags, env, null_sink, NULL);
  assert(writer);

  static const uint8_t bad_uri_buf[]   = {'f', 't', 'p', ':', 0xFF, 0x90, 0};
  static const uint8_t bad_short_buf[] = {0xFF, 0x90, 'h', 'i', 0};
  static const uint8_t bad_long_buf[]  = {'e', '\n', 'r', 0xFF, 0x90, 0};

  const SerdObjectView bad_uri = {
    SERD_URI, zix_string((const char*)bad_uri_buf), 0U, serd_no_token()};
  const SerdObjectView bad_short_lit = {
    SERD_LITERAL, zix_string((const char*)bad_short_buf), 0U, serd_no_token()};
  const SerdObjectView bad_long_lit = {
    SERD_LITERAL, zix_string((const char*)bad_long_buf), 0U, serd_no_token()};

  const SerdTokenView s = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView p = {SERD_URI, zix_string(NS_EG "p")};

  assert(write_statement(writer, 0U, s, p, bad_uri) == SERD_BAD_TEXT);
  assert(write_statement(writer, 0U, s, p, bad_short_lit) == SERD_BAD_TEXT);
  assert(write_statement(writer, 0U, s, p, bad_long_lit) == SERD_BAD_TEXT);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_strict_write(void)
{
  check_strict_write((SerdWriterFlags)SERD_WRITE_UNRESOLVED);
  check_strict_write((SerdWriterFlags)SERD_WRITE_ASCII);
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
  static const ZixStringView uri_string = ZIX_STATIC_STRING(NS_EG "u");

  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  const SerdTokenView  u = {SERD_URI, uri_string};
  const SerdObjectView o = {u.type, u.string, 0U, serd_no_token()};

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, error_sink, NULL);
  assert(writer);

  const SerdStatus st = write_statement(writer, 0U, u, u, o);
  assert(st == SERD_BAD_WRITE);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_buffer_sink(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, serd_buffer_sink, &buffer);
  assert(writer);

  assert(
    !serd_writer_set_base_uri(writer, zix_string("http://example.org/base")));
  assert(!serd_writer_finish(writer));

  char* out = serd_buffer_sink_finish(&buffer);
  assert(expect_string(out, "@base <http://example.org/base> .\n"));
  zix_free(buffer.allocator, out);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_nothing_node(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdWriter* const writer = serd_writer_new(
    world, SERD_TURTLE, SERD_WRITE_UNRESOLVED, env, null_sink, NULL);
  assert(writer);

  const SerdTokenView  s = {SERD_URI, zix_string("")};
  const SerdTokenView  p = {SERD_URI, zix_string(NS_EG "pred")};
  const SerdObjectView o = {
    SERD_NOTHING, zix_string(NS_EG "o"), 0U, {SERD_LITERAL, zix_string("")}};

  assert(write_statement(writer, 0U, s, p, o) == SERD_BAD_ARG);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_empty_syntax(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer = serd_writer_new(
    world, SERD_SYNTAX_EMPTY, 0U, env, serd_buffer_sink, &buffer);
  assert(writer);

  const SerdTokenView  s = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView  p = {SERD_URI, zix_string(NS_EG "p")};
  const SerdObjectView o = {
    SERD_URI, zix_string(NS_EG "o"), 0U, serd_no_token()};

  assert(!write_statement(writer, 0U, s, p, o));

  const char* const out = serd_buffer_sink_finish(&buffer);
  assert(out);
  assert(strlen(out) == 0);
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
check_pname_escape(const char* const lname, const char* const expected)
{
  SerdWorld* const world  = serd_world_new(NULL);
  SerdEnv* const   env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, serd_buffer_sink, &buffer);
  assert(writer);

  static const char* const prefix     = NS_EG;
  const size_t             prefix_len = strlen(prefix);

  serd_env_set_prefix(env, zix_string("eg"), zix_string(prefix));

  const SerdTokenView s = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView p = {SERD_URI, zix_string(NS_EG "p")};

  char* const uri = (char*)calloc(1, prefix_len + strlen(lname) + 1);
  assert(uri);
  memcpy(uri, prefix, prefix_len + 1);
  memcpy(uri + prefix_len, lname, strlen(lname) + 1);

  const SerdObjectView o = {SERD_URI, zix_string(uri), 0U, serd_no_token()};
  assert(!write_statement(writer, 0U, s, p, o));
  assert(!serd_writer_finish(writer));
  free(uri);

  const char* const out = serd_buffer_sink_finish(&buffer);
  assert(out);
  assert(!strcmp(out, expected));
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_env_free(env);
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
  test_new_failed_alloc();
  test_write_long_literal();
  test_write_nested_anon();
  test_writer_cleanup();
  test_write_bad_anon_stack();
  test_strict_write();
  test_write_error();
  test_buffer_sink();
  test_write_nothing_node();
  test_write_empty_syntax();
  test_write_pname_escapes();

  return 0;
}

#undef NS_EG
