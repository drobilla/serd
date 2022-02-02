/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
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
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  FILE* const temp = tmpfile();
  assert(temp);

  fprintf(temp, "_:s <http://example.org/p> _:o .\n");
  fflush(temp);
  fseek(temp, 0L, SEEK_SET);

  SerdWorld* world   = serd_world_new(&allocator.base);
  size_t     ignored = 0u;
  SerdSink*  sink    = serd_sink_new(world, &ignored, count_statements, NULL);
  SerdEnv*   env     = serd_env_new(world, serd_empty_string());

  // Successfully allocate a reader to count the number of allocations
  const size_t n_world_allocs = allocator.n_allocations;
  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0u, env, sink, 4096);
  assert(reader);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_world_allocs;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_reader_new(world, SERD_TURTLE, 0u, env, sink, 4096));
  }

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(temp);
}

static void
test_start_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  FILE* const temp = tmpfile();
  assert(temp);

  fprintf(temp, "_:s <http://example.org/p> _:o .\n");
  fflush(temp);
  fseek(temp, 0L, SEEK_SET);

  SerdWorld*  world   = serd_world_new(&allocator.base);
  size_t      ignored = 0u;
  SerdSink*   sink    = serd_sink_new(world, &ignored, count_statements, NULL);
  SerdEnv*    env     = serd_env_new(world, serd_empty_string());
  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0u, env, sink, 4096);

  assert(reader);

  SerdInputStream in = serd_open_input_stream(
    (SerdReadFunc)fread, (SerdErrorFunc)ferror, NULL, temp);

  // Successfully start a new read to count the number of allocations
  const size_t n_setup_allocs = allocator.n_allocations;
  assert(serd_reader_start(reader, &in, NULL, 4096) == SERD_SUCCESS);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations - n_setup_allocs;
  assert(!serd_reader_finish(reader));
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;

    in = serd_open_input_stream(
      (SerdReadFunc)fread, (SerdErrorFunc)ferror, NULL, temp);

    SerdStatus st = serd_reader_start(reader, &in, NULL, 4096);
    assert(st == SERD_BAD_ALLOC);
  }

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(temp);
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
test_prepare_error(void)
{
  SerdWorld* const world        = serd_world_new(NULL);
  size_t           n_statements = 0;
  FILE* const      f            = tmpfile();

  SerdSink* const sink =
    serd_sink_new(world, &n_statements, count_statements, NULL);

  assert(sink);

  SerdEnv* const    env = serd_env_new(world, serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0, env, sink, 4096);

  assert(reader);

  SerdInputStream in =
    serd_open_input_stream(prepare_test_read, prepare_test_error, NULL, f);

  assert(serd_reader_start(reader, &in, NULL, 0) == SERD_BAD_ARG);

  SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
  assert(!st);

  assert(serd_reader_read_document(reader) == SERD_BAD_READ);

  serd_close_input(&in);
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

static void
test_read_string(void)
{
  SerdWorld* world        = serd_world_new(NULL);
  size_t     n_statements = 0;

  SerdSink* sink = serd_sink_new(world, &n_statements, count_statements, NULL);
  assert(sink);

  SerdEnv* const    env = serd_env_new(world, serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0, env, sink, 4096);

  assert(reader);

  static const char* const string1 =
    "<http://example.org/s> <http://example.org/p> "
    "<http://example.org/o> .";

  const char*     position = string1;
  SerdInputStream in       = serd_open_input_string(&position);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(!serd_reader_start(reader, &in, NULL, 1));
  assert(!serd_reader_read_document(reader));
  assert(n_statements == 1);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  static const char* const string2 =
    "<http://example.org/s> <http://example.org/p> "
    "<http://example.org/o> , _:blank .";

  // Test reading a chunk
  n_statements = 0;
  position     = string2;
  in           = serd_open_input_string(&position);

  assert(!serd_reader_start(reader, &in, NULL, 1));
  assert(!serd_reader_read_chunk(reader));
  assert(n_statements == 2);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  serd_reader_free(reader);
  serd_env_free(env);
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
test_read_eof_by_page(void)
{
  FILE* const temp = tmpfile();
  assert(temp);

  fprintf(temp, "_:s <http://example.org/p> _:o .\n");
  fflush(temp);
  fseek(temp, 0L, SEEK_SET);

  SerdWorld* world   = serd_world_new(NULL);
  size_t     ignored = 0u;
  SerdSink*  sink    = serd_sink_new(world, &ignored, count_statements, NULL);
  SerdEnv*   env     = serd_env_new(world, serd_empty_string());

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0u, env, sink, 4096);

  SerdInputStream in = serd_open_input_stream(
    (SerdReadFunc)fread, (SerdErrorFunc)ferror, NULL, temp);

  assert(serd_reader_start(reader, &in, NULL, 4096) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
  fclose(temp);
}

// A byte-wise reader hits EOF once then continues (like a socket)
static void
test_read_eof_by_byte(void)
{
  SerdWorld* world   = serd_world_new(NULL);
  size_t     ignored = 0u;
  SerdSink*  sink    = serd_sink_new(world, &ignored, count_statements, NULL);
  SerdEnv*   env     = serd_env_new(world, serd_empty_string());

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0u, env, sink, 4096);

  size_t          n_reads = 0u;
  SerdInputStream in      = serd_open_input_stream(
    (SerdReadFunc)eof_test_read, (SerdErrorFunc)eof_test_error, NULL, &n_reads);

  assert(serd_reader_start(reader, &in, NULL, 1) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

static void
test_read_chunks(void)
{
  SerdWorld*        world        = serd_world_new(NULL);
  size_t            n_statements = 0;
  FILE* const       f            = tmpfile();
  static const char null         = 0;

  SerdSink* const sink =
    serd_sink_new(world, &n_statements, count_statements, NULL);

  assert(sink);

  SerdEnv* const    env = serd_env_new(world, serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0u, env, sink, 4096);

  assert(reader);

  SerdInputStream in =
    serd_open_input_stream((SerdReadFunc)fread, (SerdErrorFunc)ferror, NULL, f);

  SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
  assert(st == SERD_SUCCESS);

  // Write two statement separated by null characters
  fprintf(f, "@prefix eg: <http://example.org/> .\n");
  fprintf(f, "eg:s eg:p eg:o1 .\n");
  fwrite(&null, sizeof(null), 1, f);
  fprintf(f, "eg:s eg:p eg:o2 .\n");
  fwrite(&null, sizeof(null), 1, f);
  fseek(f, 0, SEEK_SET);

  // Read prefix
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(n_statements == 0);

  // Read first statement
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(n_statements == 1);

  // Read terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(n_statements == 1);

  // Read second statement (after null terminator)
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(n_statements == 2);

  // Read terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(n_statements == 2);

  // EOF
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(n_statements == 2);

  assert(!serd_close_input(&in));
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  fclose(f);
  serd_world_free(world);
}

/// Test that reading SERD_SYNTAX_EMPTY "succeeds" without reading any input
static void
test_read_empty(void)
{
  SerdWorld* const world        = serd_world_new(NULL);
  size_t           n_statements = 0;
  FILE* const      f            = tmpfile();

  SerdSink* const sink =
    serd_sink_new(world, &n_statements, count_statements, NULL);

  assert(sink);

  SerdEnv* const    env = serd_env_new(world, serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_SYNTAX_EMPTY, 0, env, sink, 4096);

  assert(reader);

  SerdInputStream in =
    serd_open_input_stream((SerdReadFunc)fread, (SerdErrorFunc)ferror, NULL, f);

  SerdStatus st = serd_reader_start(reader, &in, NULL, 1);
  assert(!st);

  assert(serd_reader_read_document(reader) == SERD_SUCCESS);
  assert(n_statements == 0);
  assert(!serd_reader_finish(reader));
  assert(!serd_close_input(&in));

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
    const SerdCaret* const caret =
      serd_statement_caret(event->statement.statement);
    assert(caret);

    assert(!strcmp(serd_node_string(serd_caret_name(caret)), "string"));
    assert(serd_caret_line(caret) == 1);
    assert(serd_caret_column(caret) == 47);
  }

  *called = true;
  return SERD_SUCCESS;
}

static void
test_error_cursor(void)
{
  SerdWorld* const  world  = serd_world_new(NULL);
  SerdNodes* const  nodes  = serd_world_nodes(world);
  bool              called = false;
  SerdSink*         sink   = serd_sink_new(world, &called, check_cursor, NULL);
  SerdEnv* const    env    = serd_env_new(world, serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0, env, sink, 4096);

  assert(sink);
  assert(reader);

  static const char* const string =
    "<http://example.org/s> <http://example.org/p> "
    "<http://example.org/o> .";

  const SerdNode* const string_name =
    serd_nodes_string(nodes, serd_string("string"));

  const char*     position = string;
  SerdInputStream in       = serd_open_input_string(&position);

  SerdStatus st = serd_reader_start(reader, &in, string_name, 1);
  assert(!st);
  assert(serd_reader_read_document(reader) == SERD_SUCCESS);
  assert(!serd_reader_finish(reader));
  assert(called);
  assert(!serd_close_input(&in));

  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

int
main(void)
{
  test_new_failed_alloc();
  test_start_failed_alloc();
  test_prepare_error();
  test_read_string();
  test_read_eof_by_page();
  test_read_eof_by_byte();
  test_read_chunks();
  test_read_empty();
  test_error_cursor();
  return 0;
}
