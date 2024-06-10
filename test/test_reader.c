// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/node.h>
#include <serd/reader.h>
#include <serd/statement_flags.h>
#include <serd/status.h>
#include <serd/stream.h>
#include <serd/syntax.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/filesystem.h>
#include <zix/path.h>

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int n_base;
  int n_prefix;
  int n_statement;
  int n_end;
} ReaderTest;

static SerdStatus
base_sink(void* const handle, const SerdNode* const uri)
{
  (void)uri;

  ReaderTest* const rt = (ReaderTest*)handle;
  ++rt->n_base;
  return SERD_SUCCESS;
}

static SerdStatus
prefix_sink(void* const           handle,
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
statement_sink(void* const           handle,
               SerdStatementFlags    flags,
               const SerdNode* const graph,
               const SerdNode* const subject,
               const SerdNode* const predicate,
               const SerdNode* const object,
               const SerdNode* const object_datatype,
               const SerdNode* const object_lang)
{
  (void)flags;
  (void)graph;
  (void)subject;
  (void)predicate;
  (void)object;
  (void)object_datatype;
  (void)object_lang;

  ReaderTest* const rt = (ReaderTest*)handle;
  ++rt->n_statement;
  return SERD_SUCCESS;
}

static SerdStatus
end_sink(void* const handle, const SerdNode* const node)
{
  (void)node;

  ReaderTest* const rt = (ReaderTest*)handle;
  ++rt->n_end;
  return SERD_SUCCESS;
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  ReaderTest       rt    = {0, 0, 0, 0};

  // Successfully allocate a reader to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  SerdReader* const reader = serd_reader_new(world,
                                             SERD_TURTLE,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);
  assert(reader);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_reader_new(world,
                            SERD_TURTLE,
                            0U,
                            &rt,
                            NULL,
                            base_sink,
                            prefix_sink,
                            statement_sink,
                            end_sink));
  }

  serd_reader_free(reader);
  serd_world_free(world);
}

static void
test_start_stream_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  ReaderTest       rt    = {0, 0, 0, 0};

  SerdReader* const reader = serd_reader_new(world,
                                             SERD_TURTLE,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);
  assert(reader);

  // Ensure starting succeeds with allocation available
  serd_failing_allocator_reset(&allocator, 1);
  SerdStatus st = serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdErrorFunc)ferror, NULL, "test", 4096);
  assert(!st);
  serd_reader_finish(reader);

  // Ensure starting failed without allocation available
  serd_failing_allocator_reset(&allocator, 0);
  st = serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdErrorFunc)ferror, NULL, "test", 4096);
  assert(st == SERD_BAD_ALLOC);

  serd_reader_free(reader);
  serd_world_free(world);
}

static void
test_null_callbacks(void)
{
  SerdWorld* const  world = serd_world_new(NULL);
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, NULL, NULL, NULL, NULL, NULL, NULL);

  assert(reader);
  assert(!serd_reader_handle(reader));

  assert(
    !serd_reader_start_string(reader, "_:s <http://example.org/p> _:o .\n"));

  assert(!serd_reader_read_document(reader));
  assert(!serd_reader_finish(reader));
  serd_reader_free(reader);
  serd_world_free(world);
}

static void
test_read_string(void)
{
  SerdWorld* const  world  = serd_world_new(NULL);
  ReaderTest        rt     = {0, 0, 0, 0};
  SerdReader* const reader = serd_reader_new(world,
                                             SERD_TURTLE,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);

  assert(reader);
  assert(serd_reader_handle(reader) == &rt);

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
  serd_world_free(world);
}

/// Reads a null byte after a statement, then succeeds again (like a socket)
static size_t
eof_test_read(void* const  buf,
              const size_t size,
              const size_t nmemb,
              void* const  stream)
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
eof_test_error(void* const stream)
{
  (void)stream;
  return 0;
}

/// A read of a file stream hits EOF then fails to read chunks immediately
static void
test_read_eof_file(const char* const path)
{
  FILE* const f = fopen(path, "w+b");
  assert(f);

  fprintf(f, "_:s <http://example.org/p> _:o .\n");
  fflush(f);
  fseek(f, 0L, SEEK_SET);

  SerdWorld* const  world  = serd_world_new(NULL);
  ReaderTest        rt     = {0, 0, 0, 0};
  SerdReader* const reader = serd_reader_new(world,
                                             SERD_TURTLE,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);

  fseek(f, 0L, SEEK_SET);
  serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdErrorFunc)ferror, f, "test", 4096);

  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  serd_reader_finish(reader);

  fseek(f, 0L, SEEK_SET);
  serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdErrorFunc)ferror, f, "test", 1);

  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  serd_reader_finish(reader);

  serd_reader_free(reader);
  serd_world_free(world);
  assert(!fclose(f));
  assert(!zix_remove(path));
}

// A byte-wise reader hits EOF once then continues (like a socket)
static void
test_read_eof_by_byte(void)
{
  SerdWorld* const  world  = serd_world_new(NULL);
  ReaderTest        rt     = {0, 0, 0, 0};
  SerdReader* const reader = serd_reader_new(world,
                                             SERD_TURTLE,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);

  size_t n_reads = 0U;
  serd_reader_start_stream(
    reader, eof_test_read, eof_test_error, &n_reads, "test", 1U);

  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  serd_reader_finish(reader);
  serd_reader_free(reader);
  serd_world_free(world);
}

static void
test_read_flat_chunks(const char* const path, const SerdSyntax syntax)
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
          "<http://example.org/o3> .");

  fseek(f, 0, SEEK_SET);

  SerdWorld* const  world  = serd_world_new(NULL);
  ReaderTest        rt     = {0, 0, 0, 0};
  SerdReader* const reader = serd_reader_new(world,
                                             syntax,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);

  assert(reader);
  assert(serd_reader_handle(reader) == &rt);
  assert(f);

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
  serd_reader_finish(reader);
  serd_reader_free(reader);

  serd_world_free(world);
  assert(!fclose(f));
  assert(!zix_remove(path));
}

static void
test_read_abbrev_chunks(const char* const path, const SerdSyntax syntax)
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
  fprintf(f, "eg:s eg:p [ eg:sp eg:so ] .");
  fseek(f, 0, SEEK_SET);

  SerdWorld* const  world  = serd_world_new(NULL);
  ReaderTest        rt     = {0, 0, 0, 0};
  SerdReader* const reader = serd_reader_new(world,
                                             syntax,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);

  assert(reader);
  assert(serd_reader_handle(reader) == &rt);
  assert(f);

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
  serd_reader_finish(reader);
  serd_reader_free(reader);
  serd_world_free(world);
  assert(!fclose(f));
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
  SerdWorld* const  world  = serd_world_new(NULL);
  ReaderTest        rt     = {0, 0, 0, 0};
  SerdReader* const reader = serd_reader_new(world,
                                             SERD_SYNTAX_EMPTY,
                                             0U,
                                             &rt,
                                             NULL,
                                             base_sink,
                                             prefix_sink,
                                             statement_sink,
                                             end_sink);

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
  serd_world_free(world);
}

int
main(void)
{
  char* const temp         = zix_temp_directory_path(NULL);
  char* const path_pattern = zix_path_join(NULL, temp, "serdXXXXXX");
  char* const dir          = zix_create_temporary_directory(NULL, path_pattern);
  assert(dir);

  char* const ttl_path = zix_path_join(NULL, dir, "serd_test_reader.ttl");
  char* const nq_path  = zix_path_join(NULL, dir, "serd_test_reader.nq");
  assert(ttl_path);
  assert(nq_path);

  test_new_failed_alloc();
  test_start_stream_failed_alloc();
  test_null_callbacks();
  test_read_flat_chunks(nq_path, SERD_NTRIPLES);
  test_read_flat_chunks(nq_path, SERD_NQUADS);
  test_read_abbrev_chunks(ttl_path, SERD_TURTLE);
  test_read_abbrev_chunks(ttl_path, SERD_TRIG);
  test_read_empty();
  test_read_string();
  test_read_eof_file(ttl_path);
  test_read_eof_by_byte();

  assert(!zix_remove(dir));

  zix_free(NULL, nq_path);
  zix_free(NULL, ttl_path);
  zix_free(NULL, dir);
  zix_free(NULL, path_pattern);
  zix_free(NULL, temp);
  return 0;
}
