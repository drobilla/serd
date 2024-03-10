// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

static void
check_output(SerdWriter* writer, SerdBuffer* buffer, const char* expected)
{
  serd_writer_finish(writer);
  serd_buffer_sink_finish(buffer);

  const char* output = (const char*)buffer->buf;

  assert(!strcmp(output, expected));

  buffer->len = 0;
}

static int
test(void)
{
  SerdBuffer buffer = {NULL, 0};
  SerdWorld* world  = serd_world_new();
  SerdEnv*   env    = serd_env_new(NULL);

  SerdNode b1 = serd_node_from_string(SERD_BLANK, "b1");
  SerdNode l1 = serd_node_from_string(SERD_BLANK, "l1");
  SerdNode l2 = serd_node_from_string(SERD_BLANK, "l2");
  SerdNode s1 = serd_node_from_string(SERD_LITERAL, "s1");
  SerdNode s2 = serd_node_from_string(SERD_LITERAL, "s2");

  SerdNode rdf_first = serd_node_from_string(SERD_URI, NS_RDF "first");
  SerdNode rdf_value = serd_node_from_string(SERD_URI, NS_RDF "value");
  SerdNode rdf_rest  = serd_node_from_string(SERD_URI, NS_RDF "rest");
  SerdNode rdf_nil   = serd_node_from_string(SERD_URI, NS_RDF "nil");

  serd_env_set_prefix_from_strings(env, "rdf", NS_RDF);

  SerdWriter* writer = serd_writer_new(
    world, SERD_TURTLE, 0U, env, NULL, serd_buffer_sink, &buffer);

  // Simple lone list
  serd_writer_write_statement(
    writer, SERD_TERSE_S | SERD_LIST_S, NULL, &l1, &rdf_first, &s1, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l1, &rdf_rest, &l2, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l2, &rdf_first, &s2, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l2, &rdf_rest, &rdf_nil, NULL, NULL);
  check_output(writer, &buffer, "( \"s1\" \"s2\" ) .\n");

  // Nested terse lists
  serd_writer_write_statement(writer,
                              SERD_TERSE_S | SERD_LIST_S | SERD_TERSE_O |
                                SERD_LIST_O,
                              NULL,
                              &l1,
                              &rdf_first,
                              &l2,
                              NULL,
                              NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l2, &rdf_first, &s1, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l1, &rdf_rest, &rdf_nil, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l2, &rdf_rest, &rdf_nil, NULL, NULL);
  check_output(writer, &buffer, "( ( \"s1\" ) ) .\n");

  // List as object
  serd_writer_write_statement(writer,
                              SERD_EMPTY_S | SERD_LIST_O | SERD_TERSE_O,
                              NULL,
                              &b1,
                              &rdf_value,
                              &l1,
                              NULL,
                              NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l1, &rdf_first, &s1, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l1, &rdf_rest, &l2, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l2, &rdf_first, &s2, NULL, NULL);
  serd_writer_write_statement(
    writer, 0U, NULL, &l2, &rdf_rest, &rdf_nil, NULL, NULL);
  check_output(writer, &buffer, "[] rdf:value ( \"s1\" \"s2\" ) .\n");

  serd_buffer_sink_finish(&buffer);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
  free(buffer.buf);

  return 0;
}

int
main(void)
{
  return test();
}
