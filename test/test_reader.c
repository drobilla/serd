// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/event.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/filesystem.h"
#include "zix/path.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int n_base;
  int n_prefix;
  int n_statement;
  int n_end;
} ReaderTest;

static SerdStatus
test_sink(void* const handle, const SerdEvent* const event)
{
  ReaderTest* const rt = (ReaderTest*)handle;

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

static void
test_read_string(void)
{
  SerdWorld* const world = serd_world_new();
  ReaderTest       rt    = {0, 0, 0, 0};
  SerdSink* const  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

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
  SerdSink* const   sink  = serd_sink_new(&rt, test_sink, NULL);
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, sink, 1024);
  assert(reader);
  assert(sink);

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
  assert(!zix_remove(path));
}

// A byte-wise reader hits EOF once then continues (like a socket)
static void
test_read_eof_by_byte(void)
{
  SerdWorld* const  world = serd_world_new();
  ReaderTest        rt    = {0, 0, 0, 0};
  SerdSink* const   sink  = serd_sink_new(&rt, test_sink, NULL);
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, sink, 1024);
  assert(reader);
  assert(sink);

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
  SerdSink* const  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

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
  assert(!zix_remove(path));
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
  SerdSink* const  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

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
  assert(!zix_remove(path));
}

static size_t
empty_test_read(void* buf, size_t size, size_t nmemb, void* stream)
{
  (void)buf;
  (void)size;
  (void)nmemb;

  bool* const called = (bool*)stream;

  *called = true;

  return 0;
}

static int
empty_test_error(void* stream)
{
  (void)stream;
  return 0;
}

/// Test that reading SERD_SYNTAX_EMPTY "succeeds" without reading any input
static void
test_read_empty(void)
{
  SerdWorld* const world = serd_world_new();
  ReaderTest       rt    = {0, 0, 0, 0};
  SerdSink* const  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  SerdReader* const reader =
    serd_reader_new(world, SERD_SYNTAX_EMPTY, 0U, sink, 512);
  assert(reader);

  assert(!empty_test_error(NULL));

  bool called = false;
  assert(!empty_test_read(NULL, 0U, 0U, &called));
  assert(called == true);
  called = false;

  SerdStatus st = serd_reader_start_stream(
    reader, empty_test_read, empty_test_error, &called, NULL, 1);
  assert(st == SERD_SUCCESS);

  assert(serd_reader_read_document(reader) == SERD_SUCCESS);
  assert(rt.n_statement == 0);
  assert(!called);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(rt.n_statement == 0);
  assert(!called);

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
  char* const ttl_path     = zix_path_join(NULL, dir, "serd_test_reader.ttl");
  char* const nq_path      = zix_path_join(NULL, dir, "serd_test_reader.nq");

  test_read_nquads_chunks(nq_path);
  test_read_turtle_chunks(ttl_path);
  test_read_empty();
  test_read_string();
  test_read_eof_by_page(ttl_path);
  test_read_eof_by_byte();

  assert(!zix_remove(dir));

  zix_free(NULL, nq_path);
  zix_free(NULL, ttl_path);
  zix_free(NULL, dir);
  zix_free(NULL, path_pattern);
  zix_free(NULL, temp);
  return 0;
}
