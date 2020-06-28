// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"
#include "zix/allocator.h"
#include "zix/filesystem.h"
#include "zix/path.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int n_base;
  int n_prefix;
  int n_statement;
  int n_end;
} ReaderTest;

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

SERD_PURE_FUNC
static size_t
prepare_test_read(void* buf, size_t size, size_t nmemb, void* stream)
{
  assert(size == 1);
  assert(nmemb == 1);

  (void)buf;
  (void)size;
  (void)nmemb;
  (void)stream;

  return 0;
}

static int
prepare_test_error(void* stream)
{
  (void)stream;
  return 1;
}

static void
test_prepare_error(const char* const path)
{
  SerdWorld* const world = serd_world_new();
  ReaderTest       rt    = {0, 0, 0, 0};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  fprintf(f, "_:s <http://example.org/p> _:o .\n");
  fflush(f);
  fseek(f, 0L, SEEK_SET);

  SerdSink* const sink = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0, sink, 4096);
  assert(reader);

  SerdByteSource* byte_source = serd_byte_source_new_function(
    prepare_test_read, prepare_test_error, NULL, f, NULL, 1);

  SerdStatus st = serd_reader_start(reader, byte_source);
  assert(!st);

  assert(serd_reader_read_document(reader) == SERD_ERR_UNKNOWN);

  serd_byte_source_free(byte_source);
  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
}

static void
test_read_string(void)
{
  SerdWorld* world = serd_world_new();
  ReaderTest rt    = {0, 0, 0, 0};
  SerdSink*  sink  = serd_sink_new(&rt, test_sink, NULL);
  assert(sink);

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, sink, 4096);
  assert(reader);

  SerdByteSource* byte_source =
    serd_byte_source_new_string("<http://example.org/s> <http://example.org/p> "
                                "<http://example.org/o> .",
                                NULL);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(!serd_reader_start(reader, byte_source));
  assert(!serd_reader_read_document(reader));
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 1);
  assert(rt.n_end == 0);
  assert(!serd_reader_finish(reader));

  // Test reading the same but as a chunk
  serd_byte_source_free(byte_source);
  rt.n_statement = 0;
  byte_source =
    serd_byte_source_new_string("<http://example.org/s> <http://example.org/p> "
                                "<http://example.org/o> , _:blank .",
                                NULL);

  assert(!serd_reader_start(reader, byte_source));
  assert(!serd_reader_read_chunk(reader));
  assert(rt.n_statement == 2);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  serd_reader_free(reader);
  serd_byte_source_free(byte_source);
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

  SerdWorld*  world   = serd_world_new();
  ReaderTest  ignored = {0, 0, 0, 0};
  SerdSink*   sink    = serd_sink_new(&ignored, test_sink, NULL);
  SerdReader* reader  = serd_reader_new(world, SERD_TURTLE, 0U, sink, 4096);

  SerdByteSource* byte_source = serd_byte_source_new_function(
    (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, NULL, f, NULL, 4096);

  assert(serd_reader_start(reader, byte_source) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  serd_byte_source_free(byte_source);
  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
}

// A byte-wise reader hits EOF once then continues (like a socket)
static void
test_read_eof_by_byte(void)
{
  SerdWorld*  world   = serd_world_new();
  ReaderTest  ignored = {0, 0, 0, 0};
  SerdSink*   sink    = serd_sink_new(&ignored, test_sink, NULL);
  SerdReader* reader  = serd_reader_new(world, SERD_TURTLE, 0U, sink, 4096);

  size_t          n_reads = 0U;
  SerdByteSource* byte_source =
    serd_byte_source_new_function((SerdReadFunc)eof_test_read,
                                  (SerdStreamErrorFunc)eof_test_error,
                                  NULL,
                                  &n_reads,
                                  NULL,
                                  1);

  assert(serd_reader_start(reader, byte_source) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  serd_byte_source_free(byte_source);
  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
}

static void
test_read_chunks(const char* const path)
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

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, sink, 4096);
  assert(reader);

  SerdByteSource* byte_source = serd_byte_source_new_function(
    (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, NULL, f, NULL, 1);

  SerdStatus st = serd_reader_start(reader, byte_source);
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

  serd_byte_source_free(byte_source);
  serd_reader_free(reader);
  serd_sink_free(sink);
  fclose(f);
  assert(!zix_remove(path));
  serd_world_free(world);
}

int
main(void)
{
  char* const temp         = zix_temp_directory_path(NULL);
  char* const path_pattern = zix_path_join(NULL, temp, "serdXXXXXX");
  char* const dir          = zix_create_temporary_directory(NULL, path_pattern);
  char* const path         = zix_path_join(NULL, dir, "serd_test_reader.ttl");

  test_prepare_error(path);
  test_read_string();
  test_read_eof_by_page(path);
  test_read_eof_by_byte();
  test_read_chunks(path);

  assert(!zix_remove(dir));

  zix_free(NULL, path);
  zix_free(NULL, dir);
  zix_free(NULL, path_pattern);
  zix_free(NULL, temp);
  return 0;
}
