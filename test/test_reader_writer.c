// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/buffer.h"
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

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t n_written;
  size_t error_offset;
} ErrorContext;

typedef struct {
  int n_base;
  int n_prefix;
  int n_statement;
  int n_end;
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
test_sink(void* handle, const SerdEvent* event)
{
  ReaderTest* rt = (ReaderTest*)handle;

  switch (event->type) {
  case SERD_BASE:
    ++rt->n_base;
    break;
  case SERD_PREFIX:
    ++rt->n_prefix;
    break;
  case SERD_STATEMENT:
    ++rt->n_statement;
    break;
  case SERD_END:
    ++rt->n_end;
    break;
  }

  return SERD_SUCCESS;
}

/// Reads a null byte after a statement, then succeeds again (like a socket)
static size_t
eof_test_read(void* buf, size_t size, size_t nmemb, void* stream)
{
  assert(size == 1);
  assert(nmemb == 1);
  (void)size;

  static const char* const string = "_:s1 <http://example.org/p> _:o1 .\n"
                                    "_:s2 <http://example.org/p> _:o2 .\n";

  size_t* const count = (size_t*)stream;

  // Normal reading for the first statement
  if (*count < 35) {
    *(char*)buf = string[*count];
    ++*count;
    return nmemb;
  }

  // EOF for the first read at the start of the second statement
  if (*count == 35) {
    assert(string[*count] == '_');
    ++*count;
    return 0;
  }

  if (*count >= strlen(string)) {
    return 0;
  }

  // Normal reading after the EOF, adjusting for the skipped index 35
  *(char*)buf = string[*count - 1];
  ++*count;
  return nmemb;
}

static int
eof_test_error(void* stream)
{
  (void)stream;
  return 0;
}

static void
test_read_nquads_chunks(const char* const path)
{
  static const char null = 0;

  FILE* const f = fopen(path, "w+b");
  assert(f);

  // Write two statements, a null separator, then another statement

  fprintf(f,
          "<http://example.org/s> <http://example.org/p1> "
          "<http://example.org/o1> .\n");

  fprintf(f,
          "<http://example.org/s> <http://example.org/p2> "
          "<http://example.org/o2> .\n");

  fwrite(&null, sizeof(null), 1, f);

  fprintf(f,
          "<http://example.org/s> <http://example.org/p3> "
          "<http://example.org/o3> .\n");

  fseek(f, 0, SEEK_SET);

  SerdWorld* const world = serd_world_new();
  ReaderTest       rt    = {0, 0, 0, 0};
  SerdSink* const  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  SerdReader* const reader = serd_reader_new(world, SERD_NQUADS, sink, 4096);
  assert(reader);

  SerdStatus st = serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, f, NULL, 1);
  assert(st == SERD_SUCCESS);

  // Read first statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 1);
  assert(rt.n_end == 0);

  // Read second statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 2);
  assert(rt.n_end == 0);

  // Read terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 2);
  assert(rt.n_end == 0);

  // Read last statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 3);
  assert(rt.n_end == 0);

  // EOF
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 3);
  assert(rt.n_end == 0);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
  remove(path);
}

static void
test_read_turtle_chunks(const char* const path)
{
  static const char null = 0;

  FILE* const f = fopen(path, "w+b");
  assert(f);

  // Write two statements separated by null characters
  fprintf(f, "@base <http://example.org/base/> .\n");
  fprintf(f, "@prefix eg: <http://example.org/> .\n");
  fprintf(f, "eg:s eg:p1 eg:o1 ;\n");
  fprintf(f, "     eg:p2 eg:o2 .\n");
  fwrite(&null, sizeof(null), 1, f);
  fprintf(f, "eg:s eg:p [ eg:sp eg:so ] .\n");
  fwrite(&null, sizeof(null), 1, f);
  fseek(f, 0, SEEK_SET);

  SerdWorld* world = serd_world_new();
  ReaderTest rt    = {0, 0, 0, 0};
  SerdSink*  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, sink, 4096);
  assert(reader);

  SerdStatus st = serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, f, NULL, 1);
  assert(st == SERD_SUCCESS);

  // Read base
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_base == 1);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 0);
  assert(rt.n_end == 0);

  // Read prefix
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_base == 1);
  assert(rt.n_prefix == 1);
  assert(rt.n_statement == 0);
  assert(rt.n_end == 0);

  // Read first two statements
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_base == 1);
  assert(rt.n_prefix == 1);
  assert(rt.n_statement == 2);
  assert(rt.n_end == 0);

  // Read terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_base == 1);
  assert(rt.n_prefix == 1);
  assert(rt.n_statement == 2);
  assert(rt.n_end == 0);

  // Read statements after null terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_base == 1);
  assert(rt.n_prefix == 1);
  assert(rt.n_statement == 4);
  assert(rt.n_end == 1);

  // Read terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_base == 1);
  assert(rt.n_prefix == 1);
  assert(rt.n_statement == 4);
  assert(rt.n_end == 1);

  // EOF
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_base == 1);
  assert(rt.n_prefix == 1);
  assert(rt.n_statement == 4);
  assert(rt.n_end == 1);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
  remove(path);
}

static void
test_read_string(void)
{
  SerdWorld* world = serd_world_new();
  ReaderTest rt    = {0, 0, 0, 0};
  SerdSink*  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, sink, 4096);
  assert(reader);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(
    !serd_reader_start_string(reader,
                              "<http://example.org/s> <http://example.org/p> "
                              "<http://example.org/o> .",
                              NULL));

  assert(!serd_reader_read_document(reader));
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 1);
  assert(rt.n_end == 0);
  assert(!serd_reader_finish(reader));

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
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
test_write_errors(void)
{
  SerdWorld* const world = serd_world_new();
  ErrorContext     ctx   = {0U, 0U};

  const size_t max_offsets[] = {0, 386, 1911, 2003, 386};

  // Test errors at different offsets to hit different code paths
  for (unsigned s = 1; s <= (unsigned)SERD_TRIG; ++s) {
    const SerdSyntax syntax = (SerdSyntax)s;
    for (size_t o = 0; o < max_offsets[s]; ++o) {
      ctx.n_written    = 0;
      ctx.error_offset = o;

      SerdEnv* const    env = serd_env_new(serd_empty_string());
      SerdWriter* const writer =
        serd_writer_new(world, syntax, 0U, env, faulty_sink, &ctx);

      const SerdSink* const sink = serd_writer_sink(writer);
      SerdReader* const reader = serd_reader_new(world, SERD_TRIG, sink, 4096U);

      SerdStatus st = serd_reader_start_string(reader, doc_string, NULL);
      assert(!st);
      st = serd_reader_read_document(reader);
      assert(st == SERD_BAD_WRITE);

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
  FILE*    fd  = fopen(path, "wb");
  SerdEnv* env = serd_env_new(serd_empty_string());
  assert(fd);

  SerdWorld* world = serd_world_new();

  SerdWriter* writer = serd_writer_new(
    world, SERD_TURTLE, SERD_WRITE_LAX, env, (SerdWriteFunc)fwrite, fd);
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
  SerdNode* const t = serd_new_typed_literal(serd_string("t"), urn_Type);
  SerdNode* const l = serd_new_plain_literal(serd_string("l"), en);

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

  serd_node_free(lit);
  serd_node_free(o);
  serd_node_free(t);
  serd_node_free(l);

  // Test buffer sink
  SerdBuffer buffer = {NULL, 0};
  writer =
    serd_writer_new(world, SERD_TURTLE, 0, env, serd_buffer_sink, &buffer);

  SerdNode* const base = serd_new_uri(serd_string("http://example.org/base"));

  serd_sink_write_base(serd_writer_sink(writer), base);

  serd_node_free(base);
  serd_writer_free(writer);
  char* out = serd_buffer_sink_finish(&buffer);

  assert(!strcmp(out, "@base <http://example.org/base> .\n"));
  serd_free(out);

  serd_node_free(p);
  serd_node_free(s);

  serd_env_free(env);
  serd_world_free(world);
  fclose(fd);
}

static void
test_reader(const char* path)
{
  SerdWorld*      world = serd_world_new();
  ReaderTest      rt    = {0, 0, 0, 0};
  SerdSink* const sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  // Test that too little stack space fails gracefully
  assert(!serd_reader_new(world, SERD_TURTLE, sink, 32));

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, sink, 4096);
  assert(reader);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_document(reader) == SERD_FAILURE);

  serd_reader_add_blank_prefix(reader, "tmp");

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnonnull"
#endif
  serd_reader_add_blank_prefix(reader, NULL);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

  assert(serd_reader_start_file(reader, "http://notafile", false));
  assert(serd_reader_start_file(reader, "file://invalid", false));
  assert(serd_reader_start_file(reader, "file:///nonexistant", false));

  assert(!serd_reader_start_file(reader, path, true));
  assert(!serd_reader_read_document(reader));
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 6);
  assert(rt.n_end == 0);
  assert(!serd_reader_finish(reader));

  // A read of a big page hits EOF then fails to read chunks immediately
  {
    FILE* const in = fopen(path, "rb");

    serd_reader_start_stream(
      reader, (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, in, NULL, 4096);

    assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

    serd_reader_finish(reader);
    fclose(in);
  }

  // A byte-wise reader that hits EOF once then continues (like a socket)
  {
    size_t n_reads = 0;
    serd_reader_start_stream(reader,
                             (SerdReadFunc)eof_test_read,
                             (SerdStreamErrorFunc)eof_test_error,
                             &n_reads,
                             NULL,
                             1);

    assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
    assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  }

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
}

int
main(void)
{
#ifdef _WIN32
  char         tmp[MAX_PATH] = {0};
  const size_t tmp_len       = (size_t)GetTempPath(sizeof(tmp), tmp);
#else
  const char* const env_tmp = getenv("TMPDIR");
  const char* const tmp     = env_tmp ? env_tmp : "/tmp";
  const size_t      tmp_len = strlen(tmp);
#endif

  const char* const ttl_name     = "serd_test_reader_writer.ttl";
  const char* const nq_name      = "serd_test_reader_writer.nq";
  const size_t      ttl_name_len = strlen(ttl_name);
  const size_t      nq_name_len  = strlen(nq_name);
  const size_t      path_len     = tmp_len + 1 + ttl_name_len;
  char* const       path         = (char*)calloc(path_len + 1, 1);

  memcpy(path, tmp, tmp_len + 1);
  path[tmp_len] = '/';

  memcpy(path + tmp_len + 1, nq_name, nq_name_len + 1);
  test_read_nquads_chunks(path);

  memcpy(path + tmp_len + 1, ttl_name, ttl_name_len + 1);
  test_read_turtle_chunks(path);

  test_read_string();
  test_write_errors();

  test_writer(path);
  test_reader(path);

  assert(!remove(path));
  free(path);

  printf("Success\n");
  return 0;
}
