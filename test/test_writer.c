// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/buffer.h>
#include <serd/env.h>
#include <serd/node.h>
#include <serd/statement_flags.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_EG "http://example.org/"

static size_t
null_sink(const void* const buf, const size_t len, void* const stream)
{
  (void)buf;
  (void)stream;

  return len;
}

static void
test_new_failed_alloc(void)
{
  static const SerdWriterFlags flags = (SerdWriterFlags)SERD_WRITE_BULK;

  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  SerdEnv* const   env   = serd_env_new(&allocator.base, NULL);

  // Successfully allocate a writer to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, flags, env, NULL, null_sink, NULL);
  assert(writer);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(
      !serd_writer_new(world, SERD_TURTLE, flags, env, NULL, null_sink, NULL));
  }

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_long_literal(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer = serd_writer_new(
    world, SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);
  assert(writer);

  SerdNode s = serd_node_from_string(SERD_URI, NS_EG "s");
  SerdNode p = serd_node_from_string(SERD_URI, NS_EG "p");
  SerdNode o = serd_node_from_string(SERD_LITERAL, "hello \"\"\"world\"\"\"!");

  assert(!serd_writer_write_statement(writer, 0, NULL, &s, &p, &o, NULL, NULL));
  assert(!serd_writer_finish(writer));

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

  char* const out = serd_buffer_sink_finish(&buffer);
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
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer = serd_writer_new(
    world, SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);
  assert(writer);

  SerdNode s0  = serd_node_from_string(SERD_URI, NS_EG "s0");
  SerdNode p0  = serd_node_from_string(SERD_URI, NS_EG "p0");
  SerdNode b0  = serd_node_from_string(SERD_BLANK, "b0");
  SerdNode p1  = serd_node_from_string(SERD_URI, NS_EG "p1");
  SerdNode b1  = serd_node_from_string(SERD_BLANK, "b1");
  SerdNode p2  = serd_node_from_string(SERD_URI, NS_EG "p2");
  SerdNode o2  = serd_node_from_string(SERD_URI, NS_EG "o2");
  SerdNode p3  = serd_node_from_string(SERD_URI, NS_EG "p3");
  SerdNode p4  = serd_node_from_string(SERD_URI, NS_EG "p4");
  SerdNode o4  = serd_node_from_string(SERD_URI, NS_EG "o4");
  SerdNode nil = serd_node_from_string(
    SERD_URI, "http://www.w3.org/1999/02/22-rdf-syntax-ns#nil");

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, &s0, &p0, &b0, NULL, NULL));

  assert(!serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, &b0, &p1, &b1, NULL, NULL));

  assert(
    !serd_writer_write_statement(writer, 0U, NULL, &b1, &p2, &o2, NULL, NULL));

  assert(
    !serd_writer_write_statement(writer, 0U, NULL, &b1, &p3, &nil, NULL, NULL));

  assert(!serd_writer_end_anon(writer, &b1));
  assert(
    !serd_writer_write_statement(writer, 0U, NULL, &b0, &p4, &o4, NULL, NULL));

  assert(!serd_writer_end_anon(writer, &b0));
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
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, NULL, null_sink, NULL);
  assert(writer);

  SerdNode s = serd_node_from_string(SERD_URI, NS_EG "s");
  SerdNode p = serd_node_from_string(SERD_URI, NS_EG "p");

  char     o_buf[12] = {'b', '0', '\0'};
  SerdNode o         = serd_node_from_string(SERD_BLANK, o_buf);

  st = serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, &s, &p, &o, NULL, NULL);

  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 1U; !st && i < 9U; ++i) {
    char next_o_buf[12] = {'\0'};
    snprintf(next_o_buf, sizeof(next_o_buf), "b%u", i);

    SerdNode next_o = serd_node_from_string(SERD_BLANK, next_o_buf);

    st = serd_writer_write_statement(
      writer, SERD_ANON_O, NULL, &o, &p, &next_o, NULL, NULL);

    assert(!st);

    memcpy(o_buf, next_o_buf, sizeof(o_buf));
  }

  // Finish writing without terminating nodes
  st = serd_writer_finish(writer);
  assert(!st);

  // Set the base to an empty URI
  st = serd_writer_set_base_uri(writer, NULL);
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
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, NULL, null_sink, NULL);
  assert(writer);

  SerdNode s  = serd_node_from_string(SERD_URI, NS_EG "s");
  SerdNode p  = serd_node_from_string(SERD_URI, NS_EG "p");
  SerdNode b0 = serd_node_from_string(SERD_BLANK, "b0");
  SerdNode b1 = serd_node_from_string(SERD_BLANK, "b1");
  SerdNode b2 = serd_node_from_string(SERD_BLANK, "b2");

  st = serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, &s, &p, &b0, NULL, NULL);
  assert(!st);

  // (missing call to end the anonymous node here)

  st = serd_writer_write_statement(
    writer, SERD_ANON_O, NULL, &b1, &p, &b2, NULL, NULL);

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
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, flags, env, NULL, null_sink, NULL);
  assert(writer);

  static const uint8_t bad_uri_buf[]   = {'f', 't', 'p', ':', 0xFF, 0x90, 0};
  static const uint8_t bad_short_buf[] = {0xFF, 0x90, 'h', 'i', 0};
  static const uint8_t bad_long_buf[]  = {'e', '\n', 'r', 0xFF, 0x90, 0};

  SerdNode s       = serd_node_from_string(SERD_URI, NS_EG "s");
  SerdNode p       = serd_node_from_string(SERD_URI, NS_EG "p");
  SerdNode bad_uri = serd_node_from_string(SERD_URI, (const char*)bad_uri_buf);
  SerdNode bad_short_lit =
    serd_node_from_string(SERD_LITERAL, (const char*)bad_short_buf);
  SerdNode bad_long_lit =
    serd_node_from_string(SERD_LITERAL, (const char*)bad_long_buf);

  assert(serd_writer_write_statement(
           writer, 0, NULL, &s, &p, &bad_uri, NULL, NULL) == SERD_BAD_TEXT);

  assert(serd_writer_write_statement(
           writer, 0, NULL, &s, &p, &bad_short_lit, NULL, NULL) ==
         SERD_BAD_TEXT);

  assert(serd_writer_write_statement(
           writer, 0, NULL, &s, &p, &bad_long_lit, NULL, NULL) ==
         SERD_BAD_TEXT);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_strict_write(void)
{
  check_strict_write((SerdWriterFlags)SERD_WRITE_UNRESOLVED);
  check_strict_write(
    (SerdWriterFlags)(SERD_WRITE_UNRESOLVED | SERD_WRITE_ASCII));
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
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, NULL, error_sink, NULL);
  assert(writer);

  SerdNode u = serd_node_from_string(SERD_URI, NS_EG "u");

  const SerdStatus st =
    serd_writer_write_statement(writer, 0U, NULL, &u, &u, &u, NULL, NULL);
  assert(st == SERD_BAD_WRITE);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_buffer_sink(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer = serd_writer_new(
    world, SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);
  assert(writer);

  const SerdNode base =
    serd_node_from_string(SERD_URI, "http://example.org/base");
  assert(!serd_writer_set_base_uri(writer, &base));
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
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdWriter* const writer = serd_writer_new(
    world, SERD_TURTLE, SERD_WRITE_UNRESOLVED, env, NULL, null_sink, NULL);
  assert(writer);

  SerdNode s = serd_node_from_string(SERD_URI, "");
  SerdNode p = serd_node_from_string(SERD_URI, "http://example.org/pred");
  SerdNode o = serd_node_from_string(SERD_NOTHING, "");
  assert(serd_writer_write_statement(writer, 0, NULL, &s, &p, &o, NULL, NULL) ==
         SERD_BAD_ARG);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_empty_syntax(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, NULL);

  SerdNode s = serd_node_from_string(SERD_URI, NS_EG "s");
  SerdNode p = serd_node_from_string(SERD_URI, NS_EG "p");
  SerdNode o = serd_node_from_string(SERD_CURIE, "eg:o");

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdWriter* const writer = serd_writer_new(
    world, SERD_SYNTAX_EMPTY, 0U, env, NULL, serd_buffer_sink, &buffer);
  assert(writer);

  assert(
    !serd_writer_write_statement(writer, 0U, NULL, &s, &p, &o, NULL, NULL));

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
  SerdEnv* const   env    = serd_env_new(NULL, NULL);
  SerdBuffer       buffer = {NULL, NULL, 0};

  SerdWriter* const writer = serd_writer_new(
    world, SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);
  assert(writer);

  static const char* const prefix     = NS_EG;
  const size_t             prefix_len = strlen(prefix);

  serd_env_set_prefix_from_strings(env, "eg", NS_EG);

  SerdNode s = serd_node_from_string(SERD_URI, NS_EG "s");
  SerdNode p = serd_node_from_string(SERD_URI, NS_EG "p");

  char* const uri = (char*)calloc(1, prefix_len + strlen(lname) + 1);
  assert(uri);
  memcpy(uri, prefix, prefix_len + 1);
  memcpy(uri + prefix_len, lname, strlen(lname) + 1);

  SerdNode o = serd_node_from_string(SERD_URI, uri);

  assert(
    !serd_writer_write_statement(writer, 0U, NULL, &s, &p, &o, NULL, NULL));
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
