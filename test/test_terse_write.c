/*
  Copyright 2019-2020 David Robillard <d@drobilla.net>

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
#include <stdlib.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

static void
check_output(SerdWriter* writer, SerdBuffer* buffer, const char* expected)
{
  serd_writer_finish(writer);
  serd_buffer_sink_finish(buffer);

  const char* output = (const char*)buffer->buf;
  const int   valid  = !strcmp(output, expected);
  if (valid) {
    fprintf(stderr, "%s", output);
  } else {
    fprintf(stderr, "error: Invalid output:\n%s", output);
    fprintf(stderr, "note: Expected output:\n%s", expected);
  }
  assert(valid);
  buffer->len = 0;
}

static int
test(void)
{
  SerdBuffer buffer = {NULL, 0};
  SerdWorld* world  = serd_world_new();
  SerdEnv*   env    = serd_env_new(SERD_EMPTY_STRING());
  SerdNodes* nodes  = serd_nodes_new();

  const SerdNode* b1 =
    serd_nodes_manage(nodes, serd_new_blank(SERD_STATIC_STRING("b1")));
  const SerdNode* l1 =
    serd_nodes_manage(nodes, serd_new_blank(SERD_STATIC_STRING("l1")));
  const SerdNode* l2 =
    serd_nodes_manage(nodes, serd_new_blank(SERD_STATIC_STRING("l2")));
  const SerdNode* s1 =
    serd_nodes_manage(nodes, serd_new_string(SERD_STATIC_STRING("s1")));
  const SerdNode* s2 =
    serd_nodes_manage(nodes, serd_new_string(SERD_STATIC_STRING("s2")));
  const SerdNode* rdf_first =
    serd_nodes_manage(nodes, serd_new_uri(SERD_STATIC_STRING(NS_RDF "first")));
  const SerdNode* rdf_rest =
    serd_nodes_manage(nodes, serd_new_uri(SERD_STATIC_STRING(NS_RDF "rest")));
  const SerdNode* rdf_nil =
    serd_nodes_manage(nodes, serd_new_uri(SERD_STATIC_STRING(NS_RDF "nil")));
  const SerdNode* rdf_value =
    serd_nodes_manage(nodes, serd_new_uri(SERD_STATIC_STRING(NS_RDF "value")));

  serd_env_set_prefix(
    env, SERD_STATIC_STRING("rdf"), SERD_STATIC_STRING(NS_RDF));

  SerdWriter* writer = serd_writer_new(
    world, SERD_TURTLE, 0, env, (SerdWriteFunc)serd_buffer_sink, &buffer);

  const SerdSink* sink = serd_writer_sink(writer);

  // Simple lone list
  serd_sink_write(sink, SERD_TERSE_S | SERD_LIST_S, l1, rdf_first, s1, NULL);
  serd_sink_write(sink, 0, l1, rdf_rest, l2, NULL);
  serd_sink_write(sink, 0, l2, rdf_first, s2, NULL);
  serd_sink_write(sink, 0, l2, rdf_rest, rdf_nil, NULL);
  check_output(writer, &buffer, "( \"s1\" \"s2\" ) .\n");

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
  check_output(writer, &buffer, "( ( \"s1\" ) ) .\n");

  // List as object
  serd_sink_write(
    sink, SERD_EMPTY_S | SERD_LIST_O | SERD_TERSE_O, b1, rdf_value, l1, NULL);
  serd_sink_write(sink, 0, l1, rdf_first, s1, NULL);
  serd_sink_write(sink, 0, l1, rdf_rest, l2, NULL);
  serd_sink_write(sink, 0, l2, rdf_first, s2, NULL);
  serd_sink_write(sink, 0, l2, rdf_rest, rdf_nil, NULL);
  check_output(writer, &buffer, "[]\n\trdf:value ( \"s1\" \"s2\" ) .\n");

  serd_buffer_sink_finish(&buffer);
  serd_writer_free(writer);
  serd_nodes_free(nodes);
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
