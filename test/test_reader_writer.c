// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/input_stream.h"
#include "serd/memory.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/filesystem.h"
#include "zix/path.h"
#include "zix/string_view.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  size_t n_written;
  size_t error_offset;
} ErrorContext;

typedef struct {
  int n_statement;
} ReaderTest;

static const char* const doc_string =
  "@base <http://drobilla.net/> .\n"
  "@prefix eg: <http://example.org/> .\n"
  "eg:g {\n"
  "<http://example.com/s> eg:p \"l\\n\\\"it\" ,\n"
  "  \"\"\"long\"\"\" ,\n"
  "  \"lang\"@en ;\n"
  "  eg:p <http://example.com/o> .\n"
  "}\n"
  "@prefix other: <http://example.org/other> .\n"
  "@base <http://drobilla.net/> .\n"
  "eg:s\n"
  "  <http://example.org/p> [\n"
  "    eg:p 3.0 ,\n"
  "      4 ,\n"
  "      \"lit\" ,\n"
  "      _:n42 ,\n"
  "      \"t\"^^eg:T\n"
  "  ] ;\n"
  "  eg:p () ;\n"
  "  eg:p\\!q (\"s\" 1 2.0 \"l\"@en eg:o) .\n"
  "[] eg:p eg:o .\n"
  "[ eg:p eg:o ] eg:q eg:r .\n"
  "( eg:o ) eg:t eg:u .\n";

static SerdStatus
test_sink(void* const handle, const SerdEvent* const event)
{
  ReaderTest* const rt = (ReaderTest*)handle;

  assert(event->type == SERD_STATEMENT);

  ++rt->n_statement;
  return SERD_SUCCESS;
}

static size_t
faulty_sink(const void* const buf,
            const size_t      size,
            const size_t      nmemb,
            void* const       stream)
{
  (void)buf;
  (void)size;
  (void)nmemb;

  assert(size == 1);

  ErrorContext* const ctx           = (ErrorContext*)stream;
  const size_t        new_n_written = ctx->n_written + nmemb;
  if (new_n_written >= ctx->error_offset) {
    errno = EINVAL;
    return 0U;
  }

  ctx->n_written += nmemb;
  errno = 0;
  return nmemb;
}

static void
test_output_stream(void)
{
  SerdOutputStream output = serd_open_output_file("/does/not/exist");
  assert(!output.stream);
  assert(!output.write);
  assert(!output.error);
  assert(!output.close);
}

static void
test_write_errors(void)
{
  SerdWorld* const world = serd_world_new();
  ErrorContext     ctx   = {0U, 0U};

  const SerdLimits limits = {1024, 4};
  serd_world_set_limits(world, limits);

  const size_t max_offsets[] = {0, 417, 1911, 2003, 459};

  // Test errors at different offsets to hit different code paths
  for (unsigned s = 1; s <= (unsigned)SERD_TRIG; ++s) {
    const SerdSyntax syntax = (SerdSyntax)s;
    for (size_t o = 0; o < max_offsets[s]; ++o) {
      ctx.n_written    = 0;
      ctx.error_offset = o;

      SerdEnv* const   env = serd_env_new(zix_empty_string());
      SerdOutputStream out =
        serd_open_output_stream(faulty_sink, NULL, NULL, &ctx);

      SerdWriter* const writer =
        serd_writer_new(world, syntax, 0U, env, &out, 1U);

      const SerdSink* const sink = serd_writer_sink(writer);
      SerdReader* const reader   = serd_reader_new(world, SERD_TRIG, 0U, sink);

      const char*     position = doc_string;
      SerdInputStream in       = serd_open_input_string(&position);

      SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
      assert(!st);
      st = serd_reader_read_document(reader);
      assert(st == SERD_BAD_WRITE);

      assert(!serd_close_input(&in));
      serd_reader_free(reader);
      serd_writer_free(writer);
      serd_env_free(env);
    }
  }

  serd_world_free(world);
}

static void
test_writer(const char* const path)
{
  SerdWorld* world = serd_world_new();
  SerdEnv*   env   = serd_env_new(zix_empty_string());

  SerdOutputStream output = serd_open_output_file(path);

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, SERD_WRITE_LAX, env, &output, 1U);

  assert(writer);

  serd_writer_chop_blank_prefix(writer, "tmp");
  serd_writer_chop_blank_prefix(writer, NULL);

  const SerdSink* const iface = serd_writer_sink(writer);

  // Check that writing a literal where a resource is required fails
  {
    SerdNode* const lit = serd_new_string(zix_string("hello"));
    assert(serd_sink_write_base(iface, lit));
    assert(serd_sink_write_prefix(iface, lit, lit));
    assert(serd_sink_write_end(iface, lit));
    serd_node_free(lit);
  }

  static const uint8_t       bad_buf[]    = {0xEF, 0xBF, 0xBD, 0};
  static const ZixStringView bad_buf_view = {(const char*)bad_buf, 3};

  SerdNode* const s   = serd_new_uri(zix_string("http://example.org"));
  SerdNode* const p   = serd_new_uri(zix_string("http://example.org/pred"));
  SerdNode* const bad = serd_new_string(bad_buf_view);
  assert(s);
  assert(p);
  assert(bad);

  // Write 3 invalid statements (should write nothing)
  const SerdNode* junk[][3] = {{s, bad, bad}, {bad, p, bad}, {s, bad, p}};
  for (size_t i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 3); ++i) {
    assert(serd_sink_write(iface, 0, junk[i][0], junk[i][1], junk[i][2], NULL));
  }

  serd_node_free(bad);

  {
    SerdNode* const urn_Type = serd_new_uri(zix_string("urn:Type"));
    SerdNode* const en       = serd_new_string(zix_string("en"));
    assert(urn_Type);
    assert(en);

    SerdNode* const o = serd_new_string(zix_string("o"));
    SerdNode* const t = serd_new_typed_literal(zix_string("t"), urn_Type);
    SerdNode* const l = serd_new_plain_literal(zix_string("l"), en);
    assert(o);
    assert(t);
    assert(l);

    const SerdNode* good[][3] = {{s, p, o}, {s, p, t}, {s, p, l}};

    for (size_t i = 0; i < sizeof(good) / (sizeof(SerdNode*) * 3); ++i) {
      assert(
        !serd_sink_write(iface, 0, good[i][0], good[i][1], good[i][2], NULL));
    }

    serd_node_free(l);
    serd_node_free(t);
    serd_node_free(o);
    serd_node_free(en);
    serd_node_free(urn_Type);
  }

  static const uint8_t     bad_str_buf[] = {0xFF, 0x90, 'h', 'i', 0};
  static const uint8_t     bad_uri_buf[] = {'f', 't', 'p', ':', 0xFF, 0x90, 0};
  static const char* const bad_lit_str   = (const char*)bad_str_buf;
  static const char* const bad_uri_str   = (const char*)bad_uri_buf;

  // Write statements with bad UTF-8 (should be replaced)
  SerdNode* const bad_lit = serd_new_string(zix_string(bad_lit_str));
  SerdNode* const bad_uri = serd_new_uri(zix_string(bad_uri_str));
  assert(!serd_sink_write(iface, 0, s, p, bad_lit, 0));
  assert(!serd_sink_write(iface, 0, s, p, bad_uri, 0));
  serd_node_free(bad_uri);
  serd_node_free(bad_lit);

  // Write 1 valid statement
  SerdNode* const hello = serd_new_string(zix_string("hello"));
  assert(!serd_sink_write(iface, 0, s, p, hello, 0));
  assert(!serd_writer_finish(writer));
  serd_node_free(hello);

  serd_writer_free(writer);
  serd_close_output(&output);

  // Test buffer sink
  SerdBuffer      buffer = {NULL, 0};
  SerdNode* const base   = serd_new_uri(zix_string("http://example.org/base"));

  output = serd_open_output_buffer(&buffer);
  writer = serd_writer_new(world, SERD_TURTLE, 0, env, &output, 1U);

  serd_sink_write_base(serd_writer_sink(writer), base);

  serd_node_free(base);
  serd_writer_free(writer);
  serd_close_output(&output);

  char* const out = (char*)buffer.buf;
  assert(out);
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
  SerdWorld* const world = serd_world_new();
  ReaderTest       rt    = {0};
  SerdSink* const  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  const SerdLimits limits = {512, 4};
  serd_world_set_limits(world, limits);

  // Test that too little stack space fails gracefully
  const SerdLimits old_limits  = serd_world_limits(world);
  const SerdLimits tiny_limits = {32, 4};
  serd_world_set_limits(world, tiny_limits);
  assert(!serd_reader_new(world, SERD_TURTLE, 0U, sink));

  // Restore limits and successfully create reader
  serd_world_set_limits(world, old_limits);
  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, sink);
  assert(reader);

  assert(serd_reader_read_chunk(reader) == SERD_BAD_CALL);
  assert(serd_reader_read_document(reader) == SERD_BAD_CALL);

  serd_reader_add_blank_prefix(reader, "tmp");

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnonnull"
#endif
  serd_reader_add_blank_prefix(reader, NULL);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

  SerdInputStream in = serd_open_input_file(path);
  assert(!serd_reader_start(reader, &in, NULL, 4096));
  assert(!serd_reader_read_document(reader));
  assert(rt.n_statement == 6);
  assert(!serd_reader_finish(reader));
  serd_close_input(&in);

  serd_reader_free(reader);
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

  assert(temp);
  assert(path_pattern);
  assert(dir);
  assert(path);

  test_output_stream();
  test_write_errors();

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
