// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/caret_view.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/stream_result.h"
#include "serd/syntax.h"
#include "serd/tee.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/filesystem.h"
#include "zix/path.h"
#include "zix/string_view.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int n_event;
  int n_base;
  int n_prefix;
  int n_statement;
  int n_end;
} ReaderTest;

static SerdStatus
test_sink(void* const handle, const SerdEvent* const event)
{
  ReaderTest* const rt = (ReaderTest*)handle;

  ++rt->n_event;

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
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world   = serd_world_new(&allocator.base);
  SerdEnv* const   env     = serd_env_new(&allocator.base, zix_empty_string());
  size_t           ignored = 0U;
  SerdSink* const  sink =
    serd_sink_new(&allocator.base, &ignored, test_sink, NULL);

  // Successfully allocate a reader to count the number of allocations
  const size_t n_world_allocs = allocator.n_allocations;
  SerdReader*  reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink);
  assert(reader);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_world_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_reader_new(world, SERD_TURTLE, 0U, env, sink));
  }

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

static SerdInputStream
open_file_input(FILE* const file)
{
  return serd_open_input_stream(serd_fread_wrapper, NULL, file);
}

static void
test_start_failed_alloc(const char* const path)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  FILE* const f = fopen(path, "w+b");
  assert(f);

  fprintf(f, "_:s <http://example.org/p> _:o .\n");
  fflush(f);
  fseek(f, 0L, SEEK_SET);

  SerdWorld*  world   = serd_world_new(&allocator.base);
  SerdEnv*    env     = serd_env_new(&allocator.base, zix_empty_string());
  size_t      ignored = 0U;
  SerdSink*   sink = serd_sink_new(&allocator.base, &ignored, test_sink, NULL);
  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink);
  assert(reader);

  SerdInputStream in = open_file_input(f);

  // Successfully start a new read to count the number of allocations
  const size_t n_setup_allocs = allocator.n_allocations;
  assert(serd_reader_start(reader, &in, NULL, 4096) == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_setup_allocs;
  assert(!serd_reader_finish(reader));
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;

    in = open_file_input(f);

    SerdStatus st = serd_reader_start(reader, &in, NULL, 4096);
    assert(st == SERD_BAD_ALLOC);
  }

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
}

static void
test_start_closed(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());
  ReaderTest       rt    = {0, 0, 0, 0, 0};
  SerdSink* const  sink  = serd_sink_new(NULL, &rt, test_sink, NULL);
  assert(sink);

  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0, env, sink);
  assert(reader);

  SerdInputStream  in = {NULL, NULL, NULL};
  const SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
  assert(st == SERD_BAD_ARG);
  assert(rt.n_event == 0);

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_env_free(env);
  serd_world_free(world);
}

ZIX_PURE_FUNC static SerdStreamResult
prepare_test_read(void* const stream, const size_t len, void* const buf)
{
  assert(len == 1U);

  (void)buf;
  (void)len;
  (void)stream;

  const SerdStreamResult r = {SERD_BAD_STREAM, 0U};
  return r;
}

static void
test_prepare_error(const char* const path)
{
  SerdWorld* const world = serd_world_new(NULL);
  ReaderTest       rt    = {0, 0, 0, 0, 0};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  fprintf(f, "_:s <http://example.org/p> _:o .\n");
  fflush(f);
  fseek(f, 0L, SEEK_SET);

  SerdSink* const sink = serd_sink_new(NULL, &rt, test_sink, NULL);
  assert(sink);

  SerdEnv* const    env    = serd_env_new(NULL, zix_empty_string());
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0, env, sink);
  assert(reader);

  SerdInputStream in = serd_open_input_stream(prepare_test_read, NULL, f);

  assert(serd_reader_start(reader, &in, NULL, 0) == SERD_BAD_ARG);

  SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
  assert(!st);

  // Check that starting twice fails gracefully
  assert(serd_reader_start(reader, &in, NULL, 1) == SERD_BAD_CALL);

  assert(serd_reader_read_document(reader) == SERD_BAD_STREAM);

  serd_close_input(&in);
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
  assert(!zix_remove(path));
}

static void
test_read_string(void)
{
  static const SerdLimits limits = {1024, 1};

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest      rt   = {0, 0, 0, 0, 0};
  SerdSink* const sink = serd_sink_new(NULL, &rt, test_sink, NULL);
  assert(sink);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());
  assert(env);

  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink);
  assert(reader);

  static const char* const string1 =
    "<http://example.org/s> <http://example.org/p> "
    "<http://example.org/o> .";

  const char*     position = string1;
  SerdInputStream in       = serd_open_input_string(&position);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(!serd_reader_start(reader, &in, NULL, 1));
  assert(!serd_reader_read_document(reader));
  assert(rt.n_base == 0);
  assert(rt.n_prefix == 0);
  assert(rt.n_statement == 1);
  assert(rt.n_end == 0);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  static const char* const string2 =
    "<http://example.org/s> <http://example.org/p> "
    "<http://example.org/o> , _:blank .";

  // Test reading a chunk
  rt.n_statement = 0;
  position       = string2;
  in             = serd_open_input_string(&position);

  assert(!serd_reader_start(reader, &in, NULL, 1));
  assert(!serd_reader_read_chunk(reader));
  assert(rt.n_statement == 2);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

/// A read of a big page hits EOF then fails to read chunks immediately
static void
test_read_eof_by_page(const char* const path)
{
  static const SerdLimits limits = {1024, 1};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  fprintf(f, "_:s <http://example.org/p> _:o .\n");
  fflush(f);
  fseek(f, 0L, SEEK_SET);

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest        rt     = {0, 0, 0, 0, 0};
  SerdSink* const   sink   = serd_sink_new(NULL, &rt, test_sink, NULL);
  SerdEnv* const    env    = serd_env_new(NULL, zix_empty_string());
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink);
  SerdInputStream   in     = open_file_input(f);
  assert(reader);
  assert(sink);

  assert(serd_reader_start(reader, &in, NULL, 4096) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(rt.n_event == 1);
  assert(rt.n_statement == 1);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  serd_reader_finish(reader);
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
  assert(!zix_remove(path));
}

static void
test_read_flat_chunks(const char* const path, const SerdSyntax syntax)
{
  static const SerdLimits limits = {1024, 1};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  // Write one statement and rewind to the start
  fprintf(f, "_:s <http://example.org/p1> _:o1 .\n");
  assert(!fseek(f, 0, SEEK_SET));

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest      rt   = {0, 0, 0, 0, 0};
  SerdSink* const sink = serd_sink_new(NULL, &rt, test_sink, NULL);
  assert(sink);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());
  assert(env);

  SerdReader* const reader = serd_reader_new(world, syntax, 0U, env, sink);
  assert(reader);

  SerdInputStream in = open_file_input(f);

  SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
  assert(st == SERD_SUCCESS);

  // Read first statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 1);
  assert(rt.n_statement == 1);

  // Read EOF
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(feof(f));
  assert(rt.n_event == 1);

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

  // Read EOF again
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_event == 3);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_close_input(&in));
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(f);
  assert(!zix_remove(path));
}

static void
test_read_abbrev_chunks(const char* const path, const SerdSyntax syntax)
{
  static const SerdLimits limits = {1024, 2};

  FILE* const f = fopen(path, "w+b");
  assert(f);

  // Write two directives and two statements
  fprintf(f, "@base <http://example.org/base/> .\n");
  fprintf(f, "@prefix eg: <http://example.org/> .\n");
  fprintf(f, "eg:s eg:p1 eg:o1 ;\n");
  fprintf(f, "     eg:p2 eg:o2 .\n");
  fseek(f, 0, SEEK_SET);

  SerdWorld* const world    = serd_world_new(NULL);
  ReaderTest       rt       = {0, 0, 0, 0, 0};
  SerdSink* const  out_sink = serd_sink_new(NULL, &rt, test_sink, NULL);
  assert(out_sink);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());
  assert(env);

  SerdSink* const sink = serd_tee_new(NULL, serd_env_sink(env), out_sink);
  assert(sink);

  serd_world_set_limits(world, limits);

  SerdReader* const reader = serd_reader_new(world, syntax, 0U, env, sink);
  assert(reader);

  SerdInputStream in = open_file_input(f);

  SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
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

  // Read 2 new statements
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt.n_event == 7);
  assert(rt.n_statement == 4);
  assert(rt.n_end == 1);

  // Read EOF again
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt.n_event == 7);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_close_input(&in));
  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_env_free(env);
  serd_sink_free(out_sink);
  serd_world_free(world);
  fclose(f);
  assert(!zix_remove(path));
}

/// Test that reading SERD_SYNTAX_EMPTY "succeeds" without reading any input
static void
test_read_empty(const char* const path)
{
  static const SerdLimits limits = {512, 1};

  SerdWorld* const world = serd_world_new(NULL);
  serd_world_set_limits(world, limits);

  ReaderTest      rt   = {0, 0, 0, 0, 0};
  SerdSink* const sink = serd_sink_new(NULL, &rt, test_sink, NULL);
  assert(sink);

  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());
  assert(env);

  SerdReader* const reader =
    serd_reader_new(world, SERD_SYNTAX_EMPTY, 0U, env, sink);
  assert(reader);

  FILE* const f = fopen(path, "w+b");
  assert(f);

  SerdInputStream in = open_file_input(f);

  SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
  assert(!st);

  assert(serd_reader_read_document(reader) == SERD_SUCCESS);
  assert(rt.n_statement == 0);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(rt.n_statement == 0);

  fclose(f);
  assert(!zix_remove(path));
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

static SerdStatus
check_cursor(void* handle, const SerdEvent* event)
{
  bool* const called = (bool*)handle;

  if (event->type == SERD_STATEMENT) {
    const SerdCaretView caret = event->statement.caret;
    assert(caret.document);
    assert(!strcmp(serd_node_string(caret.document), "string"));
    assert(caret.line == 1);
    assert(caret.column == 47);
  }

  *called = true;
  return SERD_SUCCESS;
}

static void
test_error_cursor(void)
{
  SerdWorld* const  world  = serd_world_new(NULL);
  SerdEnv* const    env    = serd_env_new(NULL, zix_empty_string());
  bool              called = false;
  SerdSink* const   sink   = serd_sink_new(NULL, &called, check_cursor, NULL);
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink);
  assert(sink);
  assert(reader);

  static const char* const string =
    "<http://example.org/s> <http://example.org/p> "
    "<http://example.org/o> .";

  SerdNode* const string_name = serd_node_new(NULL, serd_a_string("string"));
  const char*     position    = string;
  SerdInputStream in          = serd_open_input_string(&position);

  SerdStatus st = serd_reader_start(reader, &in, string_name, 1);
  assert(!st);
  assert(serd_reader_read_document(reader) == SERD_SUCCESS);
  assert(!serd_reader_finish(reader));
  assert(called);
  assert(!serd_close_input(&in));

  serd_node_free(NULL, string_name);
  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_env_free(env);
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

  test_new_failed_alloc();
  test_start_failed_alloc(ttl_path);
  test_start_closed();
  test_read_flat_chunks(nq_path, SERD_NTRIPLES);
  test_read_flat_chunks(nq_path, SERD_NQUADS);
  test_read_abbrev_chunks(ttl_path, SERD_TURTLE);
  test_read_abbrev_chunks(ttl_path, SERD_TRIG);
  test_read_empty(ttl_path);
  test_prepare_error(ttl_path);
  test_read_string();
  test_read_eof_by_page(ttl_path);
  test_error_cursor();

  assert(!zix_remove(dir));

  zix_free(NULL, nq_path);
  zix_free(NULL, ttl_path);
  zix_free(NULL, dir);
  zix_free(NULL, path_pattern);
  zix_free(NULL, temp);
  return 0;
}
