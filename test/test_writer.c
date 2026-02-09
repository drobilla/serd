// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"
#include "failing_allocator.h"

#include <serd/buffer.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/output_stream.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/stream_result.h>
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

static SerdStreamResult
null_sink(void* const stream, const size_t len, const void* const buf)
{
  (void)buf;
  (void)stream;

  const SerdStreamResult r = {SERD_SUCCESS, len};
  return r;
}

static const SerdOutputStream null_out = {NULL, null_sink, NULL};

static SerdObjectView
token_to_object(const SerdTokenView token)
{
  return serd_token_object_view(token);
}

static SerdStatus
write_statement(const SerdSink* const sink,
                const SerdEventFlags  flags,
                const SerdTokenView   subject,
                const SerdTokenView   predicate,
                const SerdObjectView  object)
{
  return serd_sink_event(
    sink,
    serd_statement_event(flags, serd_triple_view(subject, predicate, object)));
}

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
test_new_failed_stack(void)
{
  static const SerdLimits limits = {0, 64};

  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  serd_world_set_limits(world, limits);
  assert(!serd_writer_new(world, SERD_TURTLE, 0U, env, &null_out, 1));

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  SerdEnv* const   env   = serd_env_new(&allocator.base, zix_empty_string());

  // Successfully allocate a writer to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &null_out, 512);
  assert(writer);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_writer_new(world, SERD_TURTLE, 0U, env, &null_out, 512));
  }

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_stack_push_overflow(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdBuffer        buffer = {NULL, NULL, 0};
  SerdOutputStream  output = serd_open_output_buffer(&buffer);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  const SerdTokenView p = {SERD_URI, zix_string(NS_EG "p")};

  SerdStatus st = SERD_SUCCESS;
  for (unsigned i = 0; !st && i < 1024; ++i) {
    char label1[32] = {0};
    char label2[32] = {0};
    snprintf(label1, sizeof(label1), "b%u", i);
    snprintf(label2, sizeof(label2), "b%u", i + 1);

    const SerdTokenView s = {SERD_BLANK, zix_string(label1)};
    const SerdTokenView o = {SERD_BLANK, zix_string(label2)};
    st = write_statement(sink, SERD_ANON_O, s, p, token_to_object(o));
  }

  assert(st == SERD_BAD_STACK);
  assert(!serd_writer_finish(writer));
  assert(!serd_close_output(&output));

  static const char* const expected_prefix = "_:b0\n"
                                             "\t<http://example.org/p> [\n"
                                             "\t\t<http://example.org/p> [\n";

  const char* const out = (const char*)buffer.buf;
  assert(out);
  assert(!strncmp(out, expected_prefix, strlen(expected_prefix)));
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_stack_set_overflow(void)
{
  static const SerdLimits limits = {0, 128};

  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());
  serd_world_set_limits(world, limits);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &null_out, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  const SerdTokenView s = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView o = {SERD_URI, zix_string(NS_EG "o")};

  SerdStatus st = SERD_SUCCESS;
  for (unsigned i = 0; !st && i < 512; ++i) {
    char uri[1024] = {0};
    snprintf(uri, sizeof(uri) - 1, NS_EG "p%0*u", (int)i + 1, i);

    st = write_statement(sink,
                         0U,
                         s,
                         serd_token_view(SERD_URI, zix_string(uri)),
                         token_to_object(o));
  }

  assert(st == SERD_BAD_STACK);
  assert(!serd_writer_finish(writer));

  serd_writer_free(writer);
  serd_env_free(env);
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

  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const SerdEvent event = {(SerdEventType)42, 0U, {{"", 0}, 0, 0}, {{"", 0}}};
  assert(serd_sink_event(serd_writer_sink(writer), event) == SERD_BAD_ARG);

  assert(!serd_close_output(&output));

  const char* const out = (const char*)buffer.buf;
  assert(expect_string(out, ""));
  zix_free(buffer.allocator, buffer.buf);

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_bad_prefix(void)
{
  SerdWorld*       world  = serd_world_new(NULL);
  SerdEnv*         env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);
  SerdWriter*      writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);

  assert(writer);

  static const ZixStringView name = ZIX_STATIC_STRING("eg");
  static const ZixStringView uri  = ZIX_STATIC_STRING("rel");

  assert(serd_sink_event(serd_writer_sink(writer),
                         serd_prefix_event(name, uri)) == SERD_BAD_ARG);

  assert(!serd_close_output(&output));

  const char* const out = (const char*)buffer.buf;
  assert(out);
  assert(!strcmp(out, ""));
  zix_free(buffer.allocator, buffer.buf);

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

  const SerdTokenView s  = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView p1 = {SERD_URI, zix_string(NS_EG "p")};
  const SerdTokenView p2 = {
    SERD_URI,
    zix_string(NS_EG "http://example.org/dramatically/longer/predicate")};

  const SerdTokenView o = {SERD_BLANK, zix_string("o")};

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);
  assert(sink);

  // Successfully write a statement to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  assert(!write_statement(sink, 0U, s, p1, token_to_object(o)));
  assert(!write_statement(sink, 0U, s, p2, token_to_object(o)));
  const size_t n_write_allocs = serd_failing_allocator_reset(&allocator, 0);

  // Test that each allocation failing is handled gracefully
  for (size_t i = 0; i < n_write_allocs; ++i) {
    const SerdStatus st0 = serd_writer_finish(writer);
    assert(st0 == SERD_SUCCESS || st0 == SERD_BAD_WRITE ||
           st0 == SERD_BAD_ALLOC);
    serd_failing_allocator_reset(&allocator, i);

    buffer.len = 0U;

    const SerdStatus st1 = write_statement(sink, 0U, s, p1, token_to_object(o));
    const SerdStatus st2 = write_statement(sink, 0U, s, p2, token_to_object(o));

    assert(st1 == SERD_BAD_ALLOC || st1 == SERD_BAD_WRITE ||
           st2 == SERD_BAD_ALLOC || st2 == SERD_BAD_WRITE);
  }

  serd_writer_free(writer);
  (void)serd_close_output(&output);
  zix_free(buffer.allocator, buffer.buf);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_long_literal(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());

  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  const SerdTokenView  s = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView  p = {SERD_URI, zix_string(NS_EG "p")};
  const SerdObjectView o = {SERD_LITERAL,
                            zix_string("hello \"\"\"world\"\"\"!"),
                            SERD_HAS_QUOTE,
                            serd_no_token()};

  assert(!serd_sink_event(serd_writer_sink(writer),
                          serd_statement_event(0U, serd_triple_view(s, p, o))));
  assert(!serd_writer_finish(writer));
  assert(!serd_close_output(&output));

  static const char* const expected =
    "<http://example.org/s>\n"
    "\t<http://example.org/p> \"\"\"hello \"\"\\\"world\"\"\\\"!\"\"\" .\n";

  const char* const out = (const char*)buffer.buf;
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
  SerdOutputStream  output = serd_open_output_buffer(&buffer);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

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

  assert(!write_statement(sink, SERD_ANON_O, s0, p0, token_to_object(b0)));
  assert(!write_statement(sink, SERD_ANON_O, b0, p1, token_to_object(b1)));
  assert(!write_statement(sink, 0U, b1, p2, token_to_object(o2)));
  assert(!write_statement(sink, 0U, b1, p3, token_to_object(nil)));
  assert(!serd_sink_event(sink, serd_end_event(zix_string("b1"))));
  assert(!write_statement(sink, 0U, b0, p4, token_to_object(o4)));
  assert(!serd_sink_event(sink, serd_end_event(zix_string("b0"))));
  assert(!serd_writer_finish(writer));
  assert(!serd_close_output(&output));

  static const char* const expected =
    "<http://example.org/s0>\n"
    "\t<http://example.org/p0> [\n"
    "\t\t<http://example.org/p1> [\n"
    "\t\t\t<http://example.org/p2> <http://example.org/o2> ;\n"
    "\t\t\t<http://example.org/p3> ()\n"
    "\t\t] ;\n"
    "\t\t<http://example.org/p4> <http://example.org/o4>\n"
    "\t] .\n";

  const char* const out = (const char*)buffer.buf;
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
    serd_writer_new(world, SERD_TURTLE, 0U, env, &null_out, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  char buf[12] = {'b', '0', '\0'};

  SerdTokenView s = {SERD_URI, zix_string(NS_EG "s")};
  SerdTokenView p = {SERD_URI, zix_string(NS_EG "p")};
  SerdTokenView o = {SERD_BLANK, zix_string(buf)};

  st = write_statement(sink, SERD_ANON_O, s, p, token_to_object(o));
  assert(!st);

  // Write the start of several nested anonymous objects
  for (unsigned i = 1U; !st && i < 9U; ++i) {
    char temp[12] = {'\0'};
    snprintf(temp, sizeof(temp), "b%u", i);

    const SerdTokenView next_o = {SERD_BLANK, zix_string(temp)};

    st = write_statement(sink, SERD_ANON_O, o, p, token_to_object(next_o));
    assert(!st);

    memcpy(buf, temp, sizeof(buf));
    o.string = zix_string(buf);
  }

  // Finish writing without terminating nodes
  st = serd_writer_finish(writer);
  assert(!st);

  // Set the base to an empty URI
  st = serd_sink_event(sink, serd_base_event(zix_string("")));
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
    serd_writer_new(world, SERD_TURTLE, 0U, env, &null_out, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  SerdTokenView  s  = {SERD_URI, zix_string(NS_EG "s")};
  SerdTokenView  p  = {SERD_URI, zix_string(NS_EG "p")};
  SerdObjectView b0 = {SERD_BLANK, zix_string(NS_EG "b0"), 0U, serd_no_token()};
  SerdTokenView  b1 = {SERD_BLANK, zix_string(NS_EG "b1")};
  SerdObjectView b2 = {SERD_BLANK, zix_string(NS_EG "b2"), 0U, serd_no_token()};

  st = write_statement(sink, SERD_ANON_O, s, p, b0);
  assert(!st);

  // (missing call to end the anonymous node here)

  st = write_statement(sink, SERD_ANON_O, b1, p, b2);
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

  SerdOutputStream  out = serd_open_output_stream(null_sink, NULL, NULL);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, flags, env, &null_out, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

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

  assert(write_statement(sink, 0, s, p, bad_uri) == SERD_BAD_TEXT);
  assert(write_statement(sink, 0, s, p, bad_short_lit) == SERD_BAD_TEXT);
  assert(write_statement(sink, 0, s, p, bad_long_lit) == SERD_BAD_TEXT);

  serd_writer_free(writer);
  assert(!serd_close_output(&out));
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
static SerdStreamResult
error_sink(void* const stream, const size_t len, const void* const buf)
{
  (void)buf;
  (void)len;
  (void)stream;
  const SerdStreamResult r = {SERD_BAD_WRITE, 0U};
  return r;
}

static void
test_write_error(void)
{
  static const ZixStringView uri_string = ZIX_STATIC_STRING(NS_EG "u");

  SerdWorld* const world  = serd_world_new(NULL);
  SerdEnv* const   env    = serd_env_new(NULL, zix_empty_string());
  SerdOutputStream output = serd_open_output_stream(error_sink, NULL, NULL);

  const SerdTokenView  u = {SERD_URI, uri_string};
  const SerdObjectView o = {u.type, u.string, 0U, serd_no_token()};

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  const SerdSink* const sink = serd_writer_sink(writer);

  const SerdStatus st = write_statement(sink, 0U, u, u, o);
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
  SerdOutputStream  output = serd_open_output_buffer(&buffer);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  serd_sink_event(serd_writer_sink(writer),
                  serd_base_event(zix_string("http://example.org/base")));
  assert(!serd_writer_finish(writer));
  assert(!serd_close_output(&output));

  const char* const out = (const char*)buffer.buf;
  assert(expect_string(out, "@base <http://example.org/base> .\n"));
  zix_free(buffer.allocator, buffer.buf);

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
    world, SERD_TURTLE, SERD_WRITE_UNRESOLVED, env, &null_out, 1U);
  assert(writer);

  const SerdTokenView  s = {SERD_URI, zix_string("")};
  const SerdTokenView  p = {SERD_URI, zix_string(NS_EG "pred")};
  const SerdObjectView o = {
    SERD_NOTHING, zix_string(NS_EG "o"), 0U, {SERD_LITERAL, zix_string("")}};

  assert(serd_sink_event(serd_writer_sink(writer),
                         serd_statement_event(0U, serd_triple_view(s, p, o))) ==
         SERD_BAD_ARG);

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
  SerdOutputStream  output = serd_open_output_buffer(&buffer);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_SYNTAX_EMPTY, 0U, env, &output, 1U);
  assert(writer);

  const SerdTokenView  s = {SERD_URI, zix_string(NS_EG "s")};
  const SerdTokenView  p = {SERD_URI, zix_string(NS_EG "p")};
  const SerdObjectView o = {
    SERD_URI, zix_string(NS_EG "o"), 0U, serd_no_token()};

  assert(!serd_sink_event(serd_writer_sink(writer),
                          serd_statement_event(0U, serd_triple_view(s, p, o))));

  assert(!serd_close_output(&output));

  const char* const out = (const char*)buffer.buf;
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
  SerdOutputStream output = serd_open_output_buffer(&buffer);

  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
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

  const SerdSink* const sink = serd_writer_sink(writer);

  const SerdObjectView o = {SERD_URI, zix_string(uri), 0U, serd_no_token()};
  assert(!write_statement(sink, 0U, s, p, o));
  assert(!serd_writer_finish(writer));
  assert(!serd_close_output(&output));
  free(uri);

  const char* const out = (const char*)buffer.buf;
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
  test_writer_new();
  test_new_failed_stack();
  test_new_failed_alloc();
  test_stack_push_overflow();
  test_stack_set_overflow();
  test_write_bad_event();
  test_write_bad_prefix();
  test_write_failed_alloc();
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
