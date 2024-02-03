// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_EG "http://example.org/"

static void
test_writer_new(void)
{
  SerdWorld*       world  = serd_world_new(NULL);
  SerdEnv*         env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  assert(!serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 0U));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world  = serd_world_new(&allocator.base);
  SerdEnv*         env    = serd_env_new(&allocator.base, zix_empty_string());
  SerdBuffer       buffer = {&allocator.base, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);
  const size_t     n_world_allocs = allocator.n_allocations;

  // Successfully allocate a writer to count the number of allocations
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);

  assert(writer);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_world_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U));
  }

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld*       world  = serd_world_new(&allocator.base);
  SerdEnv*         env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {&allocator.base, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdNode* s  = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* p1 = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));

  SerdNode* p2 = serd_node_new(
    NULL,
    serd_a_uri_string("http://example.org/dramatically/longer/predicate"));

  SerdNode* o = serd_node_new(NULL, serd_a_blank_string("o"));

  const size_t n_setup_allocs = allocator.n_allocations;

  // Successfully write a statement to count the number of allocations
  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  const SerdSink* sink = serd_writer_sink(writer);
  assert(writer);
  assert(sink);
  assert(!serd_sink_write(sink, 0U, s, p1, o, NULL));
  assert(!serd_sink_write(sink, 0U, s, p2, o, NULL));
  const size_t n_new_allocs = allocator.n_allocations - n_setup_allocs;

  serd_writer_free(writer);

  // Test that each allocation failing is handled gracefully
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;

    buffer.len = 0U;
    if ((writer = serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U))) {
      sink = serd_writer_sink(writer);

      const SerdStatus st1 = serd_sink_write(sink, 0U, s, p1, o, NULL);
      const SerdStatus st2 = serd_sink_write(sink, 0U, s, p2, o, NULL);

      assert(st1 == SERD_BAD_ALLOC || st1 == SERD_BAD_WRITE ||
             st2 == SERD_BAD_ALLOC || st2 == SERD_BAD_WRITE);

      serd_writer_free(writer);
    }
  }

  serd_close_output(&output);
  zix_free(buffer.allocator, buffer.buf);
  serd_env_free(env);
  serd_node_free(NULL, o);
  serd_node_free(NULL, p2);
  serd_node_free(NULL, p1);
  serd_node_free(NULL, s);
  serd_world_free(world);
}

static void
test_write_bad_event(void)
{
  SerdWorld*       world  = serd_world_new(NULL);
  SerdEnv*         env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  const SerdEvent event = {(SerdEventType)42};
  assert(serd_sink_write_event(serd_writer_sink(writer), &event) ==
         SERD_BAD_ARG);

  assert(!serd_close_output(&output));

  char* const out = (char*)buffer.buf;
  assert(out);
  assert(!strcmp(out, ""));
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_long_literal(void)
{
  SerdWorld*       world  = serd_world_new(NULL);
  SerdEnv*         env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  SerdNode* s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode* o = serd_node_new(
    NULL,
    serd_a_literal(zix_string("hello \"\"\"world\"\"\"!"), SERD_IS_LONG, NULL));

  assert(serd_node_flags(o) & SERD_IS_LONG);
  assert(!serd_sink_write(serd_writer_sink(writer), 0, s, p, o, NULL));

  serd_node_free(NULL, o);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);

  char* const out = (char*)buffer.buf;

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

  assert(!strcmp(out, expected));
  zix_free(buffer.allocator, buffer.buf);

  serd_world_free(world);
}

static void
test_write_nested_anon(void)
{
  SerdWorld*       world  = serd_world_new(NULL);
  SerdEnv*         env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);

  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  SerdNode* s0  = serd_node_new(NULL, serd_a_uri_string(NS_EG "s0"));
  SerdNode* p0  = serd_node_new(NULL, serd_a_uri_string(NS_EG "p0"));
  SerdNode* b0  = serd_node_new(NULL, serd_a_blank_string("b0"));
  SerdNode* p1  = serd_node_new(NULL, serd_a_uri_string(NS_EG "p1"));
  SerdNode* b1  = serd_node_new(NULL, serd_a_blank_string("b1"));
  SerdNode* p2  = serd_node_new(NULL, serd_a_uri_string(NS_EG "p2"));
  SerdNode* o2  = serd_node_new(NULL, serd_a_uri_string(NS_EG "o2"));
  SerdNode* p3  = serd_node_new(NULL, serd_a_uri_string(NS_EG "p3"));
  SerdNode* p4  = serd_node_new(NULL, serd_a_uri_string(NS_EG "p4"));
  SerdNode* o4  = serd_node_new(NULL, serd_a_uri_string(NS_EG "o4"));
  SerdNode* nil = serd_node_new(
    NULL, serd_a_uri_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#nil"));

  assert(!serd_sink_write(sink, SERD_ANON_O, s0, p0, b0, NULL));
  assert(!serd_sink_write(sink, SERD_ANON_O, b0, p1, b1, NULL));
  assert(!serd_sink_write(sink, 0U, b1, p2, o2, NULL));
  assert(!serd_sink_write(sink, SERD_LIST_O, b1, p3, nil, NULL));
  assert(!serd_sink_write_end(sink, b1));
  assert(!serd_sink_write(sink, 0U, b0, p4, o4, NULL));
  assert(!serd_sink_write_end(sink, b0));

  serd_node_free(NULL, s0);
  serd_node_free(NULL, p0);
  serd_node_free(NULL, b0);
  serd_node_free(NULL, p1);
  serd_node_free(NULL, b1);
  serd_node_free(NULL, p2);
  serd_node_free(NULL, o2);
  serd_node_free(NULL, p3);
  serd_node_free(NULL, p4);
  serd_node_free(NULL, o4);
  serd_node_free(NULL, nil);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_close_output(&output);

  char* const out = (char*)buffer.buf;

  static const char* const expected =
    "<http://example.org/s0>\n"
    "\t<http://example.org/p0> [\n"
    "\t\t<http://example.org/p1> [\n"
    "\t\t\t<http://example.org/p2> <http://example.org/o2> ;\n"
    "\t\t\t<http://example.org/p3> ()\n"
    "\t\t] ;\n"
    "\t\t<http://example.org/p4> <http://example.org/o4>\n"
    "\t] .\n";

  fprintf(stderr, "%s\n", out);
  assert(!strcmp((char*)out, expected));
  zix_free(NULL, out);
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
test_writer_cleanup(void)
{
  SerdStatus       st    = SERD_SUCCESS;
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());
  SerdOutputStream output =
    serd_open_output_stream(null_sink, NULL, NULL, NULL);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);

  const SerdSink* const sink = serd_writer_sink(writer);

  SerdNode* const s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* const p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode*       o = serd_node_new(NULL, serd_a_blank_string("b0"));

  st = serd_sink_write(sink, SERD_ANON_O, s, p, o, NULL);
  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 1U; !st && i < 9U; ++i) {
    char buf[12] = {'\0'};
    snprintf(buf, sizeof(buf), "b%u", i);

    SerdNode* next_o = serd_node_new(NULL, serd_a_blank_string(buf));

    st = serd_sink_write(sink, SERD_ANON_O, o, p, next_o, NULL);

    assert(!st);

    serd_node_free(NULL, o);
    o = next_o;
  }

  // Finish writing without terminating nodes
  assert(!(st = serd_writer_finish(writer)));

  // Set the base to an empty URI
  SerdNode* empty_uri = serd_node_new(NULL, serd_a_uri_string(""));
  assert(!(st = serd_sink_write_base(sink, empty_uri)));
  serd_node_free(NULL, empty_uri);

  // Free (which could leak if the writer doesn't clean up the stack properly)
  serd_node_free(NULL, o);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
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
  SerdOutputStream output =
    serd_open_output_stream(null_sink, NULL, NULL, NULL);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);

  const SerdSink* const sink = serd_writer_sink(writer);

  SerdNode* s  = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* p  = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode* b0 = serd_node_new(NULL, serd_a_blank(zix_string("b0")));
  SerdNode* b1 = serd_node_new(NULL, serd_a_blank(zix_string("b1")));
  SerdNode* b2 = serd_node_new(NULL, serd_a_blank(zix_string("b2")));

  st = serd_sink_write(sink, SERD_ANON_O, s, p, b0, NULL);
  assert(!st);

  // (missing call to end the anonymous node here)

  st = serd_sink_write(sink, SERD_ANON_O, b1, p, b2, NULL);
  assert(st == SERD_BAD_ARG);

  st = serd_writer_finish(writer);
  assert(!st);

  serd_node_free(NULL, b2);
  serd_node_free(NULL, b1);
  serd_node_free(NULL, b0);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_strict_write(void)
{
  const char* const path = "serd_strict_write_test.ttl";
  FILE* const       fd   = fopen(path, "wb");
  assert(fd);

  SerdWorld* const  world = serd_world_new(NULL);
  SerdEnv* const    env   = serd_env_new(NULL, zix_empty_string());
  SerdOutputStream  out   = serd_open_output_stream(null_sink, NULL, NULL, fd);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &out, 1U);

  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  const uint8_t bad_str[] = {0xFF, 0x90, 'h', 'i', 0};

  SerdNode* s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));

  SerdNode* bad_lit = serd_node_new(NULL, serd_a_string((const char*)bad_str));
  SerdNode* bad_uri =
    serd_node_new(NULL, serd_a_uri_string((const char*)bad_str));

  assert(serd_sink_write(sink, 0, s, p, bad_lit, NULL) == SERD_BAD_TEXT);
  assert(serd_sink_write(sink, 0, s, p, bad_uri, NULL) == SERD_BAD_TEXT);

  serd_node_free(NULL, bad_uri);
  serd_node_free(NULL, bad_lit);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
  fclose(fd);
  remove(path);
}

// Produce a write error without setting errno
static size_t
error_sink(const void* const buf,
           const size_t      size,
           const size_t      len,
           void* const       stream)
{
  (void)buf;
  (void)size;
  (void)len;
  (void)stream;
  return 0U;
}

static void
test_write_error(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());
  SerdOutputStream out = serd_open_output_stream(error_sink, NULL, NULL, NULL);
  SerdStatus       st  = SERD_SUCCESS;

  SerdNode* const u = serd_node_new(NULL, serd_a_uri_string(NS_EG "u"));

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &out, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  st = serd_sink_write(sink, 0U, u, u, u, NULL);
  assert(st == SERD_BAD_WRITE);
  serd_writer_free(writer);

  serd_node_free(NULL, u);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_empty_syntax(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdNode* const s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* const p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode* const o = serd_node_new(NULL, serd_a_curie_string("eg:o"));

  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_SYNTAX_EMPTY, 0U, env, &output, 1U);

  assert(writer);
  assert(!serd_sink_write(serd_writer_sink(writer), 0U, s, p, o, NULL));
  assert(!serd_close_output(&output));

  char* const out = (char*)buffer.buf;
  assert(out);
  assert(strlen(out) == 0);
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_node_free(NULL, o);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_close_output(&output);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_writer_stack_overflow(void)
{
  SerdWorld* world = serd_world_new(NULL);
  SerdEnv*   env   = serd_env_new(NULL, zix_empty_string());

  SerdOutputStream output =
    serd_open_output_stream(null_sink, NULL, NULL, NULL);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);

  const SerdSink* sink = serd_writer_sink(writer);

  SerdNode* const s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* const p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));
  SerdNode*       o = serd_node_new(NULL, serd_a_blank_string("blank"));

  SerdStatus st = serd_sink_write(sink, SERD_ANON_O, s, p, o, NULL);
  assert(!st);

  // Repeatedly write nested anonymous objects until the writer stack overflows
  for (unsigned i = 0U; i < 512U; ++i) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "b%u", i);

    SerdNode* const next_o = serd_node_new(NULL, serd_a_blank_string(buf));

    st = serd_sink_write(sink, SERD_ANON_O, o, p, next_o, NULL);

    serd_node_free(NULL, o);
    o = next_o;

    if (st) {
      assert(st == SERD_BAD_STACK);
      break;
    }
  }

  assert(st == SERD_BAD_STACK);

  serd_node_free(NULL, o);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_writer_free(writer);
  serd_close_output(&output);
  serd_env_free(env);
  serd_world_free(world);
}

static void
check_pname_escape(const char* const lname, const char* const expected)
{
  SerdWorld*       world  = serd_world_new(NULL);
  SerdEnv*         env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  static const char* const prefix     = NS_EG;
  const size_t             prefix_len = strlen(prefix);

  serd_env_set_prefix(env, zix_string("eg"), zix_string(prefix));

  SerdNode* s = serd_node_new(NULL, serd_a_uri_string(NS_EG "s"));
  SerdNode* p = serd_node_new(NULL, serd_a_uri_string(NS_EG "p"));

  char* const uri = (char*)calloc(1, prefix_len + strlen(lname) + 1);
  memcpy(uri, prefix, prefix_len + 1);
  memcpy(uri + prefix_len, lname, strlen(lname) + 1);

  SerdNode* node = serd_node_new(NULL, serd_a_uri_string(uri));
  assert(!serd_sink_write(serd_writer_sink(writer), 0, s, p, node, NULL));
  serd_node_free(NULL, node);

  free(uri);
  serd_node_free(NULL, p);
  serd_node_free(NULL, s);
  serd_writer_free(writer);
  serd_close_output(&output);

  char* const out = (char*)buffer.buf;
  assert(!strcmp(out, expected));
  zix_free(buffer.allocator, buffer.buf);

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
  test_writer_new();
  test_new_failed_alloc();
  test_write_failed_alloc();
  test_write_bad_event();
  test_write_long_literal();
  test_write_nested_anon();
  test_writer_cleanup();
  test_write_bad_anon_stack();
  test_strict_write();
  test_write_error();
  test_write_empty_syntax();
  test_writer_stack_overflow();
  test_write_pname_escapes();

  return 0;
}

#undef NS_EG
