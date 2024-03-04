// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/sink.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

static void
check_output(SerdWriter* writer, SerdOutputStream* out, const char* expected)
{
  SerdBuffer* const buffer = (SerdBuffer*)out->stream;

  serd_writer_finish(writer);
  serd_close_output(out);

  const char* output = (const char*)buffer->buf;

  fprintf(stderr, "output: %s\n", output);
  fprintf(stderr, "expected: %s\n", expected);
  assert(!strcmp(output, expected));

  buffer->len = 0U;
  out->stream = buffer;
}

static int
test(void)
{
  SerdBuffer buffer = {NULL, NULL, 0};
  SerdWorld* world  = serd_world_new(NULL);
  SerdEnv*   env    = serd_env_new(NULL, zix_empty_string());

  SerdNode* b1 = serd_new_blank(NULL, zix_string("b1"));
  SerdNode* l1 = serd_new_blank(NULL, zix_string("l1"));
  SerdNode* l2 = serd_new_blank(NULL, zix_string("l2"));
  SerdNode* s1 = serd_new_string(NULL, zix_string("s1"));
  SerdNode* s2 = serd_new_string(NULL, zix_string("s2"));

  SerdNode* rdf_first = serd_new_uri(NULL, zix_string(NS_RDF "first"));
  SerdNode* rdf_value = serd_new_uri(NULL, zix_string(NS_RDF "value"));
  SerdNode* rdf_rest  = serd_new_uri(NULL, zix_string(NS_RDF "rest"));
  SerdNode* rdf_nil   = serd_new_uri(NULL, zix_string(NS_RDF "nil"));

  serd_env_set_prefix(env, zix_string("rdf"), zix_string(NS_RDF));

  SerdOutputStream  output = serd_open_output_buffer(&buffer);
  SerdWriter* const writer =
    serd_writer_new(world, SERD_TURTLE, 0U, env, &output, 1U);

  const SerdSink* sink = serd_writer_sink(writer);

  // Simple lone list
  serd_sink_write(sink, SERD_TERSE_S | SERD_LIST_S, l1, rdf_first, s1, NULL);
  serd_sink_write(sink, 0, l1, rdf_rest, l2, NULL);
  serd_sink_write(sink, 0, l2, rdf_first, s2, NULL);
  serd_sink_write(sink, 0, l2, rdf_rest, rdf_nil, NULL);
  check_output(writer, &output, "( \"s1\" \"s2\" ) .\n");

  // Nested terse lists
  serd_sink_write(sink,
                  SERD_TERSE_S | SERD_LIST_S | SERD_TERSE_O | SERD_LIST_O,
                  l1,
                  rdf_first,
                  l2,
                  NULL);
  serd_sink_write(sink, 0, l2, rdf_first, s1, NULL);
  serd_sink_write(sink, 0, l1, rdf_rest, rdf_nil, NULL);
  serd_sink_write(sink, 0, l2, rdf_rest, rdf_nil, NULL);
  check_output(writer, &output, "( ( \"s1\" ) ) .\n");

  // List as object
  serd_sink_write(
    sink, SERD_EMPTY_S | SERD_LIST_O | SERD_TERSE_O, b1, rdf_value, l1, NULL);
  serd_sink_write(sink, 0, l1, rdf_first, s1, NULL);
  serd_sink_write(sink, 0, l1, rdf_rest, l2, NULL);
  serd_sink_write(sink, 0, l2, rdf_first, s2, NULL);
  serd_sink_write(sink, 0, l2, rdf_rest, rdf_nil, NULL);
  check_output(writer, &output, "[] rdf:value ( \"s1\" \"s2\" ) .\n");

  serd_writer_free(writer);
  serd_node_free(NULL, rdf_nil);
  serd_node_free(NULL, rdf_rest);
  serd_node_free(NULL, rdf_value);
  serd_node_free(NULL, rdf_first);
  serd_node_free(NULL, s2);
  serd_node_free(NULL, s1);
  serd_node_free(NULL, l2);
  serd_node_free(NULL, l1);
  serd_node_free(NULL, b1);
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
