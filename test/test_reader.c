// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/syntax.h"
#include "serd/world.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int n_base;
  int n_prefix;
  int n_statement;
  int n_end;
} ReaderTest;

static SerdStatus
test_base_sink(void* const handle, const SerdNode* const uri)
{
  (void)uri;

  ReaderTest* const rt = (ReaderTest*)handle;
  ++rt->n_base;
  return SERD_SUCCESS;
}

static SerdStatus
test_prefix_sink(void* const           handle,
                 const SerdNode* const name,
                 const SerdNode* const uri)
{
  (void)name;
  (void)uri;

  ReaderTest* const rt = (ReaderTest*)handle;
  ++rt->n_prefix;
  return SERD_SUCCESS;
}

static SerdStatus
test_statement_sink(void* const             handle,
                    SerdStatementFlags      flags,
                    const SerdStatementView statement)
{
  (void)flags;
  (void)statement;

  ReaderTest* const rt = (ReaderTest*)handle;
  ++rt->n_statement;
  return SERD_SUCCESS;
}

static SerdStatus
test_end_sink(void* const handle, const SerdNode* const node)
{
  (void)node;

  ReaderTest* const rt = (ReaderTest*)handle;
  ++rt->n_end;
  return SERD_SUCCESS;
}

static void
test_read_string(void)
{
  SerdWorld* const world = serd_world_new();
  ReaderTest       rt    = {0, 0, 0, 0};
  SerdSink* const  sink  = serd_sink_new(&rt, NULL);
  assert(sink);

  serd_sink_set_base_func(sink, test_base_sink);
  serd_sink_set_prefix_func(sink, test_prefix_sink);
  serd_sink_set_statement_func(sink, test_statement_sink);
  serd_sink_set_end_func(sink, test_end_sink);

  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, sink, 512);
  assert(reader);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(
    !serd_reader_start_string(reader,
                              "<http://example.org/s> <http://example.org/p> "
                              "<http://example.org/o> ."));

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

/// A read of a big page hits EOF then fails to read chunks immediately
static void
test_read_eof_by_page(const char* const path)
{
  FILE* const f = fopen(path, "w+b");
  assert(f);

  fprintf(f, "_:s <http://example.org/p> _:o .\n");
  fflush(f);
  fseek(f, 0L, SEEK_SET);

  SerdWorld* const  world = serd_world_new();
  ReaderTest        rt    = {0, 0, 0, 0};
  SerdSink* const   sink  = serd_sink_new(&rt, NULL);
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, sink, 1024);
  assert(reader);
  assert(sink);

  serd_sink_set_base_func(sink, test_base_sink);
  serd_sink_set_prefix_func(sink, test_prefix_sink);
  serd_sink_set_statement_func(sink, test_statement_sink);
  serd_sink_set_end_func(sink, test_end_sink);

  serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdErrorFunc)ferror, f, NULL, 4096);

  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  serd_reader_finish(reader);
  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
}

// A byte-wise reader hits EOF once then continues (like a socket)
static void
test_read_eof_by_byte(void)
{
  SerdWorld* const  world = serd_world_new();
  ReaderTest        rt    = {0, 0, 0, 0};
  SerdSink* const   sink  = serd_sink_new(&rt, NULL);
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, sink, 1024);
  assert(reader);
  assert(sink);

  serd_sink_set_base_func(sink, test_base_sink);
  serd_sink_set_prefix_func(sink, test_prefix_sink);
  serd_sink_set_statement_func(sink, test_statement_sink);
  serd_sink_set_end_func(sink, test_end_sink);

  size_t n_reads = 0U;
  serd_reader_start_stream(
    reader, eof_test_read, eof_test_error, &n_reads, "test", 1U);

  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
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
  SerdSink* const  sink  = serd_sink_new(&rt, NULL);
  assert(sink);

  serd_sink_set_base_func(sink, test_base_sink);
  serd_sink_set_prefix_func(sink, test_prefix_sink);
  serd_sink_set_statement_func(sink, test_statement_sink);
  serd_sink_set_end_func(sink, test_end_sink);

  SerdReader* const reader =
    serd_reader_new(world, SERD_NQUADS, 0U, sink, 1024);

  assert(reader);

  SerdStatus st = serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdErrorFunc)ferror, f, NULL, 1);
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

  SerdWorld* const world = serd_world_new();
  ReaderTest       rt    = {0, 0, 0, 0};
  SerdSink* const  sink  = serd_sink_new(&rt, NULL);
  assert(sink);

  serd_sink_set_base_func(sink, test_base_sink);
  serd_sink_set_prefix_func(sink, test_prefix_sink);
  serd_sink_set_statement_func(sink, test_statement_sink);
  serd_sink_set_end_func(sink, test_end_sink);

  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, sink, 1024);
  assert(reader);

  SerdStatus st = serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdErrorFunc)ferror, f, NULL, 1);
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

  const char* const ttl_name     = "serd_test_reader.ttl";
  const char* const nq_name      = "serd_test_reader.nq";
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
  test_read_eof_by_page(path);
  test_read_eof_by_byte();
  assert(!remove(path));

  free(path);
  return 0;
}
