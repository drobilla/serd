// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"

#include <serd/buffer.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/output_stream.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

static inline SerdStatus
write_triple(const SerdSink* const sink,
             const SerdEventFlags  flags,
             const SerdTokenView   subject,
             const SerdTokenView   predicate,
             const SerdTokenView   object)
{
  assert(sink);

  const SerdObjectView o = {
    object.type, object.string, 0U, {SERD_LITERAL, zix_empty_string()}};

  return serd_sink_event(
    sink, serd_statement_event(flags, serd_triple_view(subject, predicate, o)));
}

static void
check_output(SerdWriter* writer, SerdOutputStream* out, const char* expected)
{
  SerdBuffer* const buffer = (SerdBuffer*)out->stream;

  assert(!serd_writer_finish(writer));
  assert(!serd_close_output(out));

  const char* const output = (const char*)buffer->buf;
  assert(output);
  assert(expect_string(output, expected));

  buffer->len = 0U;
  out->stream = buffer;
}

static int
test(void)
{
  SerdWorld* const world  = serd_world_new(NULL);
  SerdEnv* const   env    = serd_env_new(NULL, zix_empty_string());
  SerdBuffer       buffer = {NULL, NULL, 0};

  const SerdTokenView b1 = {SERD_BLANK, zix_string("b1")};
  const SerdTokenView b2 = {SERD_BLANK, zix_string("b2")};
  const SerdTokenView b3 = {SERD_BLANK, zix_string("b3")};
  const SerdTokenView l1 = {SERD_BLANK, zix_string("l1")};
  const SerdTokenView l2 = {SERD_BLANK, zix_string("l2")};
  const SerdTokenView s1 = {SERD_LITERAL, zix_string("s1")};
  const SerdTokenView s2 = {SERD_LITERAL, zix_string("s2")};

  const SerdTokenView rdf_first = {SERD_URI, zix_string(NS_RDF "first")};
  const SerdTokenView rdf_value = {SERD_URI, zix_string(NS_RDF "value")};
  const SerdTokenView rdf_rest  = {SERD_URI, zix_string(NS_RDF "rest")};
  const SerdTokenView rdf_nil   = {SERD_URI, zix_string(NS_RDF "nil")};

  serd_env_set_prefix(env, zix_string("rdf"), zix_string(NS_RDF));

  SerdOutputStream  output = serd_open_output_buffer(&buffer);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);
  assert(writer);

  const SerdSink* sink = serd_writer_sink(writer);

  // Simple lone list
  write_triple(sink, SERD_TERSE_S | SERD_LIST_S, l1, rdf_first, s1);
  write_triple(sink, 0U, l1, rdf_rest, l2);
  write_triple(sink, 0U, l2, rdf_first, s2);
  write_triple(sink, 0U, l2, rdf_rest, rdf_nil);
  check_output(writer, &output, "( \"s1\" \"s2\" ) .\n");

  // Nested terse lists
  write_triple(sink,
               SERD_TERSE_S | SERD_LIST_S | SERD_TERSE_O | SERD_LIST_O,
               l1,
               rdf_first,
               l2);
  write_triple(sink, 0U, l2, rdf_first, s1);
  write_triple(sink, 0U, l1, rdf_rest, rdf_nil);
  write_triple(sink, 0U, l2, rdf_rest, rdf_nil);
  check_output(writer, &output, "( ( \"s1\" ) ) .\n");

  // List as object
  write_triple(sink,
               SERD_EMPTY_S | SERD_TERSE_S | SERD_LIST_O | SERD_TERSE_O,
               b1,
               rdf_value,
               l1);
  write_triple(sink, 0U, l1, rdf_first, s1);
  write_triple(sink, 0U, l1, rdf_rest, l2);
  write_triple(sink, 0U, l2, rdf_first, s2);
  write_triple(sink, 0U, l2, rdf_rest, rdf_nil);
  check_output(writer, &output, "[] rdf:value ( \"s1\" \"s2\" ) .\n");

  // List with anonymous elements as object
  write_triple(
    sink, SERD_EMPTY_S | SERD_LIST_O | SERD_TERSE_O, b1, rdf_value, l1);
  write_triple(sink, SERD_EMPTY_O, l1, rdf_first, b2);
  write_triple(sink, 0U, l1, rdf_rest, l2);
  write_triple(sink, SERD_ANON_O, l2, rdf_first, b3);
  write_triple(sink, 0U, b3, rdf_value, s1);
  serd_sink_event(sink, serd_end_event(b3.string));
  write_triple(sink, 0U, l2, rdf_rest, rdf_nil);
  check_output(
    writer, &output, "[]\n\trdf:value ( [] [ rdf:value \"s1\" ] ) .\n");

  serd_writer_free(writer);
  zix_free(buffer.allocator, buffer.buf);
  serd_env_free(env);
  serd_world_free(world);

  return 0;
}

int
main(void)
{
  return test();
}
