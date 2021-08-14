// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SerdStatus
count_statements(void* handle, const SerdEvent* event)
{
  if (event->type == SERD_STATEMENT) {
    ++*(size_t*)handle;
  }

  return SERD_SUCCESS;
}

static void
test_writer(const char* const path)
{
  FILE*    fd  = fopen(path, "wb");
  SerdEnv* env = serd_env_new(serd_empty_string());
  assert(fd);

  SerdWorld* world = serd_world_new();
  SerdNodes* nodes = serd_world_nodes(world);

  SerdOutputStream output = serd_open_output_file(path);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, SERD_WRITE_LAX, env, &output, 1);

  assert(writer);

  const SerdNode* lit = serd_nodes_string(nodes, serd_string("hello"));

  const SerdSink* const iface = serd_writer_sink(writer);
  assert(serd_sink_write_base(iface, lit));
  assert(serd_sink_write_prefix(iface, lit, lit));
  assert(serd_sink_write_end(iface, lit));

  static const uint8_t bad_buf[]    = {0xEF, 0xBF, 0xBD, 0};
  const SerdStringView bad_buf_view = {(const char*)bad_buf, 3};

  const SerdNode* s = serd_nodes_uri(nodes, serd_string("http://example.org"));
  const SerdNode* p =
    serd_nodes_uri(nodes, serd_string("http://example.org/pred"));

  const SerdNode* bad = serd_nodes_string(nodes, bad_buf_view);

  // Write 3 invalid statements (should write nothing)
  const SerdNode* junk[][3] = {{s, bad, bad}, {bad, p, bad}, {s, bad, p}};
  for (size_t i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 3); ++i) {
    assert(serd_sink_write(iface, 0, junk[i][0], junk[i][1], junk[i][2], NULL));
  }

  const SerdStringView urn_Type = serd_string("urn:Type");
  const SerdStringView en       = serd_string("en");

  const SerdNode* const o = serd_nodes_string(nodes, serd_string("o"));
  const SerdNode* const t =
    serd_nodes_literal(nodes, serd_string("t"), SERD_HAS_DATATYPE, urn_Type);

  const SerdNode* const l =
    serd_nodes_literal(nodes, serd_string("l"), SERD_HAS_LANGUAGE, en);

  const SerdNode* good[][3] = {{s, p, o}, {s, p, t}, {s, p, l}};

  for (size_t i = 0; i < sizeof(good) / (sizeof(SerdNode*) * 3); ++i) {
    assert(
      !serd_sink_write(iface, 0, good[i][0], good[i][1], good[i][2], NULL));
  }

  static const uint8_t     bad_str_buf[] = {0xFF, 0x90, 'h', 'i', 0};
  static const uint8_t     bad_uri_buf[] = {'f', 't', 'p', ':', 0xFF, 0x90, 0};
  static const char* const bad_lit_str   = (const char*)bad_str_buf;
  static const char* const bad_uri_str   = (const char*)bad_uri_buf;

  // Write statements with bad UTF-8 (should be replaced)
  const SerdNode* bad_lit = serd_nodes_string(nodes, serd_string(bad_lit_str));
  const SerdNode* bad_uri = serd_nodes_uri(nodes, serd_string(bad_uri_str));
  assert(!serd_sink_write(iface, 0, s, p, bad_lit, 0));
  assert(!serd_sink_write(iface, 0, s, p, bad_uri, 0));

  // Write 1 valid statement
  const SerdNode* const hello = serd_nodes_string(nodes, serd_string("hello"));
  assert(!serd_sink_write(iface, 0, s, p, hello, 0));

  serd_writer_free(writer);
  serd_close_output(&output);

  // Test buffer sink
  SerdBuffer buffer = {NULL, 0};

  output = serd_open_output_buffer(&buffer);
  writer = serd_writer_new(world, SERD_TURTLE, 0, env, &output, 1);

  const SerdNode* const base =
    serd_nodes_uri(nodes, serd_string("http://example.org/base"));

  serd_writer_set_base_uri(writer, base);

  serd_writer_free(writer);
  serd_close_output(&output);
  char* out = (char*)buffer.buf;

  assert(out);
  assert(!strcmp(out, "@base <http://example.org/base> .\n"));
  serd_free(out);

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_reader(const char* path)
{
  SerdWorld*      world        = serd_world_new();
  size_t          n_statements = 0;
  SerdSink* const sink = serd_sink_new(&n_statements, count_statements, NULL);
  assert(sink);

  SerdEnv* const env = serd_env_new(serd_empty_string());
  assert(env);

  // Test that too little stack space fails gracefully
  assert(!serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 32));
  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 4096);
  assert(reader);

  assert(serd_reader_read_document(reader) == SERD_ERR_BAD_CALL);
  assert(serd_reader_read_chunk(reader) == SERD_ERR_BAD_CALL);

  SerdInputStream in = serd_open_input_file(path);
  assert(!serd_reader_start(reader, &in, NULL, 4096));
  assert(!serd_reader_read_document(reader));
  assert(n_statements == 6);
  serd_reader_finish(reader);
  serd_close_input(&in);

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

int
main(void)
{
  const char* const path = "serd_test.ttl";
  test_writer(path);
  test_reader(path);

  printf("Success\n");
  return 0;
}
