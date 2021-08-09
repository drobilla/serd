// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
#include "serd/byte_sink.h"
#include "serd/byte_source.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/memory.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/string_view.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/filesystem.h"
#include "zix/path.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int n_statement;
} ReaderTest;

static SerdStatus
test_sink(void* handle, const SerdEvent* event)
{
  ReaderTest* rt = (ReaderTest*)handle;

  assert(event->type == SERD_STATEMENT);

  ++rt->n_statement;
  return SERD_SUCCESS;
}

static void
test_writer(const char* const path)
{
  FILE*    fd  = fopen(path, "wb");
  SerdEnv* env = serd_env_new(serd_empty_string());
  assert(fd);

  SerdWorld* world = serd_world_new();

  SerdByteSink* byte_sink = serd_byte_sink_new_function(
    (SerdWriteFunc)fwrite, (SerdStreamCloseFunc)fclose, fd, 1);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, SERD_WRITE_LAX, env, byte_sink);

  assert(writer);

  serd_writer_chop_blank_prefix(writer, "tmp");
  serd_writer_chop_blank_prefix(writer, NULL);

  SerdNode* lit = serd_new_string(serd_string("hello"));

  const SerdSink* const iface = serd_writer_sink(writer);
  assert(serd_sink_write_base(iface, lit));
  assert(serd_sink_write_prefix(iface, lit, lit));
  assert(serd_sink_write_end(iface, lit));

  static const uint8_t bad_buf[]    = {0xEF, 0xBF, 0xBD, 0};
  const SerdStringView bad_buf_view = {(const char*)bad_buf, 3};

  SerdNode* s   = serd_new_uri(serd_string("http://example.org"));
  SerdNode* p   = serd_new_uri(serd_string("http://example.org/pred"));
  SerdNode* bad = serd_new_string(bad_buf_view);

  // Write 3 invalid statements (should write nothing)
  const SerdNode* junk[][3] = {{s, bad, bad}, {bad, p, bad}, {s, bad, p}};
  for (size_t i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 3); ++i) {
    assert(serd_sink_write(iface, 0, junk[i][0], junk[i][1], junk[i][2], NULL));
  }

  serd_node_free(bad);

  const SerdStringView urn_Type = serd_string("urn:Type");
  const SerdStringView en       = serd_string("en");

  SerdNode* const o = serd_new_string(serd_string("o"));

  SerdNode* const t =
    serd_new_literal(serd_string("t"), SERD_HAS_DATATYPE, urn_Type);

  SerdNode* const l = serd_new_literal(serd_string("l"), SERD_HAS_LANGUAGE, en);

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
  SerdNode* bad_lit = serd_new_string(serd_string(bad_lit_str));
  SerdNode* bad_uri = serd_new_uri(serd_string(bad_uri_str));
  assert(!serd_sink_write(iface, 0, s, p, bad_lit, 0));
  assert(!serd_sink_write(iface, 0, s, p, bad_uri, 0));
  serd_node_free(bad_uri);
  serd_node_free(bad_lit);

  // Write 1 valid statement
  SerdNode* const hello = serd_new_string(serd_string("hello"));
  assert(!serd_sink_write(iface, 0, s, p, hello, 0));
  serd_node_free(hello);

  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);

  serd_node_free(lit);
  serd_node_free(o);
  serd_node_free(t);
  serd_node_free(l);

  // Test buffer sink
  SerdBuffer buffer = {NULL, 0};

  byte_sink = serd_byte_sink_new_buffer(&buffer);
  writer    = serd_writer_new(world, SERD_TURTLE, 0, env, byte_sink);

  SerdNode* const base = serd_new_uri(serd_string("http://example.org/base"));

  serd_sink_write_base(serd_writer_sink(writer), base);

  serd_node_free(base);
  serd_writer_free(writer);
  serd_byte_sink_free(byte_sink);
  char* out = serd_buffer_sink_finish(&buffer);

  assert(!strcmp(out, "@base <http://example.org/base> .\n"));
  serd_free(out);

  serd_node_free(p);
  serd_node_free(s);

  serd_env_free(env);
  serd_world_free(world);
}

static void
test_reader(const char* path)
{
  SerdWorld*      world = serd_world_new();
  ReaderTest      rt    = {0};
  SerdSink* const sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  SerdEnv* const env = serd_env_new(serd_empty_string());
  assert(env);

  // Test that too little stack space fails gracefully
  assert(!serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 32));
  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 4096);
  assert(reader);

  assert(serd_reader_read_chunk(reader) == SERD_ERR_BAD_CALL);
  assert(serd_reader_read_document(reader) == SERD_ERR_BAD_CALL);

  serd_reader_add_blank_prefix(reader, "tmp");

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnonnull"
#endif
  serd_reader_add_blank_prefix(reader, NULL);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

  SerdByteSource* byte_source = serd_byte_source_new_filename(path, 4096);
  assert(!serd_reader_start(reader, byte_source));
  assert(!serd_reader_read_document(reader));
  assert(rt.n_statement == 6);
  assert(!serd_reader_finish(reader));
  serd_byte_source_free(byte_source);

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

int
main(void)
{
  char* const temp         = zix_temp_directory_path(NULL);
  char* const path_pattern = zix_path_join(NULL, temp, "serdXXXXXX");
  char* const dir          = zix_create_temporary_directory(NULL, path_pattern);
  char* const path         = zix_path_join(NULL, dir, "serd_test_reader.ttl");

  test_writer(path);
  test_reader(path);

  assert(!zix_remove(path));
  assert(!zix_remove(dir));

  zix_free(NULL, path);
  zix_free(NULL, dir);
  zix_free(NULL, path_pattern);
  zix_free(NULL, temp);

  printf("Success\n");
  return 0;
}
