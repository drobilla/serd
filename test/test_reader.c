// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/event.h>
#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <serd/stream.h>
#include <serd/syntax.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/filesystem.h>
#include <zix/path.h>
#include <zix/string_view.h>

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  int n_event;
  int n_base;
  int n_prefix;
  int n_statement;
  int n_end;
} ReaderTest;

static SerdStatus
event_sink(void* const handle, const SerdEvent* const event)
{
  ReaderTest* const rt = (ReaderTest*)handle;

  ++rt->n_event;

  const SerdEventType type = event->type;
  switch (type) {
  case SERD_EVENT_BASE:
    ++rt->n_base;
    break;
  case SERD_EVENT_PREFIX:
    ++rt->n_prefix;
    break;
  case SERD_EVENT_STATEMENT:
    ++rt->n_statement;
    break;
  case SERD_EVENT_END:
    ++rt->n_end;
    break;
  }

  return SERD_SUCCESS;
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  ReaderTest       rt    = {0, 0, 0, 0, 0};
  const SerdSink   sink  = {&rt, event_sink};

  // Successfully allocate a reader to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, &sink);
  assert(reader);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_reader_new(world, SERD_TURTLE, 0U, &sink));
  }

  serd_reader_free(reader);
  serd_world_free(world);
}

static void
test_start_stream_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const  world  = serd_world_new(&allocator.base);
  ReaderTest        rt     = {0, 0, 0, 0, 0};
  const SerdSink    sink   = {&rt, event_sink};
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, &sink);
  assert(reader);

  // Ensure starting succeeds with allocation available
  serd_failing_allocator_reset(&allocator, 2);
  SerdStatus st = serd_reader_start_stream(reader,
                                           (SerdReadFunc)fread,
                                           (SerdErrorFunc)ferror,
                                           NULL,
                                           zix_string("test"),
                                           4096);
  assert(!st);
  serd_reader_finish(reader);

  // Ensure starting failed without allocation available
  serd_failing_allocator_reset(&allocator, 0);
  st = serd_reader_start_stream(reader,
                                (SerdReadFunc)fread,
                                (SerdErrorFunc)ferror,
                                NULL,
                                zix_string("test"),
                                4096);
  assert(st == SERD_BAD_ALLOC);

  serd_reader_free(reader);
  serd_world_free(world);
}

static void
test_read_string(void)
{
  static const SerdLimits limits = {1024, 0};

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest        rt     = {0, 0, 0, 0, 0};
  const SerdSink    sink   = {&rt, event_sink};
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, &sink);
  assert(reader);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(
    !serd_reader_start_string(reader,
                              "<http://example.org/s> <http://example.org/p> "
                              "<http://example.org/o> .",
                              zix_empty_string()));

  assert(!serd_reader_read_document(reader));
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 1);
  assert(rt.n_end == 0);
  assert(!serd_reader_finish(reader));

  serd_reader_free(reader);
  serd_world_free(world);
}

/// A read of a file stream hits EOF then fails to read chunks immediately
static void
test_read_eof_file(const char* const path)
{
  static const SerdLimits limits = {1024, 0};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  fprintf(f, "_:s <http://example.org/p> _:o .\n");
  fflush(f);
  fseek(f, 0L, SEEK_SET);

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest        rt     = {0, 0, 0, 0, 0};
  const SerdSink    sink   = {&rt, event_sink};
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, &sink);
  assert(reader);

  fseek(f, 0L, SEEK_SET);
  serd_reader_start_stream(reader,
                           (SerdReadFunc)fread,
                           (SerdErrorFunc)ferror,
                           f,
                           zix_empty_string(),
                           4096);

  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(rt.n_event == 1);
  assert(rt.n_statement == 1);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  fseek(f, 0L, SEEK_SET);
  serd_reader_start_stream(reader,
                           (SerdReadFunc)fread,
                           (SerdErrorFunc)ferror,
                           f,
                           zix_empty_string(),
                           1);

  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  serd_reader_free(reader);
  serd_world_free(world);
  assert(!fclose(f));
  assert(!zix_remove(path));
}

static void
test_read_flat_chunks(const char* const path, const SerdSyntax syntax)
{
  static const SerdLimits limits = {2048, 0};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  // Write one statement and rewind to the start
  fprintf(f, "_:s <http://example.org/p1> _:o1 .\n");
  assert(!fseek(f, 0, SEEK_SET));

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest        rt     = {0, 0, 0, 0, 0};
  const SerdSink    sink   = {&rt, event_sink};
  SerdReader* const reader = serd_reader_new(world, syntax, 0U, &sink);
  assert(reader);

  SerdStatus st = serd_reader_start_stream(reader,
                                           (SerdReadFunc)fread,
                                           (SerdErrorFunc)ferror,
                                           f,
                                           zix_empty_string(),
                                           1);
  assert(st == SERD_SUCCESS);

  // Read first statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 1);
  assert(rt.n_statement == 1);

  // Read EOF
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_event == 1);
  assert(feof(f));

  const long eof_pos = ftell(f);

  // Write second and third statements
  fprintf(f,
          "<http://example.org/s> <http://example.org/p2> "
          "<http://example.org/o2> .\n"
          "<http://example.org/s> <http://example.org/p3> "
          "<http://example.org/o3> .\n");

  // Rewind to the no-longer-EOF position
  clearerr(f);
  assert(!fseek(f, eof_pos, SEEK_SET));

  // Read second statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 2);
  assert(rt.n_statement == 2);

  // Read third statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 3);
  assert(rt.n_statement == 3);

  // Read EOF again, twice
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_event == 3);

  serd_reader_finish(reader);
  serd_reader_free(reader);
  serd_world_free(world);
  assert(!fclose(f));
  assert(!zix_remove(path));
}

static void
test_read_abbrev_chunks(const char* const path, const SerdSyntax syntax)
{
  static const SerdLimits limits = {2048, 0};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  // Write two directives and two statements
  fprintf(f, "@base <http://example.org/base/> .\n");
  fprintf(f, "@prefix eg: <http://example.org/> .\n");
  fprintf(f, "eg:s eg:p1 eg:o1 ;\n");
  fprintf(f, "     eg:p2 eg:o2 .\n");
  fseek(f, 0, SEEK_SET);

  SerdWorld* const world = serd_world_new(NULL);
  assert(world);
  serd_world_set_limits(world, limits);

  ReaderTest        rt     = {0, 0, 0, 0, 0};
  const SerdSink    sink   = {&rt, event_sink};
  SerdReader* const reader = serd_reader_new(world, syntax, 0U, &sink);
  assert(reader);

  SerdStatus st = serd_reader_start_stream(reader,
                                           (SerdReadFunc)fread,
                                           (SerdErrorFunc)ferror,
                                           f,
                                           zix_empty_string(),
                                           1);
  assert(st == SERD_SUCCESS);

  // Read base
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 1);
  assert(rt.n_base == 1);

  // Read prefix
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 2);
  assert(rt.n_prefix == 1);

  // Read first two statements
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 4);
  assert(rt.n_statement == 2);

  // Read EOF
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_event == 4);

  const long eof_pos = ftell(f);

  // Write 2 more statements
  fprintf(f, "eg:s eg:p [ eg:sp eg:so ] .\n");
  clearerr(f);
  assert(!fseek(f, eof_pos, SEEK_SET));

  // Read 2 new statements and anonymous node end
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 7);
  assert(rt.n_statement == 4);
  assert(rt.n_end == 1);

  // Read EOF again, twice
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_event == 7);

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
  static const SerdLimits limits = {512, 0};

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest        rt   = {0, 0, 0, 0, 0};
  const SerdSink    sink = {&rt, event_sink};
  SerdReader* const reader =
    serd_reader_new(world, SERD_SYNTAX_EMPTY, 0U, &sink);
  assert(reader);

  assert(!empty_test_error(NULL));

  bool called = false;
  assert(!empty_test_read(NULL, 0U, 0U, &called));
  assert(called == true);
  called = false;

  SerdStatus st = serd_reader_start_stream(
    reader, empty_test_read, empty_test_error, &called, zix_empty_string(), 1);
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

  assert(dir);

  test_new_failed_alloc();
  test_start_stream_failed_alloc();
  test_read_flat_chunks(nq_path, SERD_NTRIPLES);
  test_read_flat_chunks(nq_path, SERD_NQUADS);
  test_read_abbrev_chunks(ttl_path, SERD_TURTLE);
  test_read_abbrev_chunks(ttl_path, SERD_TRIG);
  test_read_empty();
  test_read_string();
  test_read_eof_file(ttl_path);

  assert(!zix_remove(dir));

  zix_free(NULL, nq_path);
  zix_free(NULL, ttl_path);
  zix_free(NULL, dir);
  zix_free(NULL, path_pattern);
  zix_free(NULL, temp);
  return 0;
}
