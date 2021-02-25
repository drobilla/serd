// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

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
  SerdWorld* const world        = serd_world_new();
  size_t           n_statements = 0;
  FILE* const      f            = tmpfile();

  SerdSink* const sink = serd_sink_new(&n_statements, count_statements, NULL);
  assert(sink);

  SerdEnv* const    env = serd_env_new(serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0, env, sink, 4096);

  assert(reader);

  SerdByteSource* byte_source = serd_byte_source_new_function(
    prepare_test_read, prepare_test_error, NULL, f, NULL, 1);

  SerdStatus st = serd_reader_start(reader, byte_source);
  assert(!st);

  assert(serd_reader_read_document(reader) == SERD_ERR_UNKNOWN);

  serd_byte_source_free(byte_source);
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

static void
test_read_string(void)
{
  SerdWorld* world        = serd_world_new();
  size_t     n_statements = 0;

  SerdSink* sink = serd_sink_new(&n_statements, count_statements, NULL);
  assert(sink);

  SerdEnv* const    env = serd_env_new(serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 4096);

  assert(reader);

  SerdByteSource* byte_source =
    serd_byte_source_new_string("<http://example.org/s> <http://example.org/p> "
                                "<http://example.org/o> .",
                                NULL);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(!serd_reader_start(reader, byte_source));
  assert(!serd_reader_read_document(reader));
  assert(n_statements == 1);
  assert(!serd_reader_finish(reader));

  // Test reading the same but as a chunk
  serd_byte_source_free(byte_source);
  n_statements = 0;
  byte_source =
    serd_byte_source_new_string("<http://example.org/s> <http://example.org/p> "
                                "<http://example.org/o> , _:blank .",
                                NULL);

  assert(!serd_reader_start(reader, byte_source));
  assert(!serd_reader_read_chunk(reader));
  assert(n_statements == 2);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  serd_reader_free(reader);
  serd_env_free(env);
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
test_read_eof_by_page(void)
{
  FILE* const temp = tmpfile();
  assert(temp);

  fprintf(temp, "_:s <http://example.org/p> _:o .\n");
  fflush(temp);
  fseek(temp, 0L, SEEK_SET);

  SerdWorld* world   = serd_world_new();
  size_t     ignored = 0U;
  SerdSink*  sink    = serd_sink_new(&ignored, count_statements, NULL);
  SerdEnv*   env     = serd_env_new(serd_empty_string());

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 4096);

  SerdByteSource* byte_source = serd_byte_source_new_function(
    (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, NULL, temp, NULL, 4096);

  assert(serd_reader_start(reader, byte_source) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(!serd_reader_finish(reader));

  serd_byte_source_free(byte_source);
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
  SerdWorld* world   = serd_world_new();
  size_t     ignored = 0U;
  SerdSink*  sink    = serd_sink_new(&ignored, count_statements, NULL);
  SerdEnv*   env     = serd_env_new(serd_empty_string());

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 4096);

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
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

static void
test_read_chunks(void)
{
  SerdWorld*        world        = serd_world_new();
  size_t            n_statements = 0;
  FILE* const       f            = tmpfile();
  static const char null         = 0;

  SerdSink* const sink = serd_sink_new(&n_statements, count_statements, NULL);
  assert(sink);

  SerdEnv* const    env = serd_env_new(serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0U, env, sink, 4096);

  assert(reader);

  SerdByteSource* byte_source = serd_byte_source_new_function(
    (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, NULL, f, NULL, 1);

  SerdStatus st = serd_reader_start(reader, byte_source);
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

  serd_byte_source_free(byte_source);
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  fclose(f);
  serd_world_free(world);
}

static size_t
empty_test_read(void* buf, size_t size, size_t nmemb, void* stream)
{
  (void)buf;
  (void)size;
  (void)nmemb;
  (void)stream;

  assert(false);

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
  SerdWorld* const world        = serd_world_new();
  size_t           n_statements = 0;
  FILE* const      f            = tmpfile();

  SerdSink* const sink = serd_sink_new(&n_statements, count_statements, NULL);
  assert(sink);

  SerdEnv* const    env = serd_env_new(serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_SYNTAX_EMPTY, 0, env, sink, 4096);

  assert(reader);

  SerdByteSource* byte_source = serd_byte_source_new_function(
    empty_test_read, empty_test_error, NULL, f, NULL, 1);

  SerdStatus st = serd_reader_start(reader, byte_source);
  assert(!st);

  assert(serd_reader_read_document(reader) == SERD_SUCCESS);
  assert(n_statements == 0);

  serd_byte_source_free(byte_source);
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
  SerdWorld*        world  = serd_world_new();
  bool              called = false;
  SerdSink*         sink   = serd_sink_new(&called, check_cursor, NULL);
  SerdEnv* const    env    = serd_env_new(serd_empty_string());
  SerdReader* const reader =
    serd_reader_new(world, SERD_TURTLE, 0, env, sink, 4096);

  assert(sink);
  assert(reader);

  SerdByteSource* byte_source =
    serd_byte_source_new_string("<http://example.org/s> <http://example.org/p> "
                                "<http://example.org/o> .",
                                NULL);

  SerdStatus st = serd_reader_start(reader, byte_source);
  assert(!st);
  assert(serd_reader_read_document(reader) == SERD_SUCCESS);
  assert(called);

  serd_byte_source_free(byte_source);
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(sink);
  serd_world_free(world);
}

int
main(void)
{
  test_prepare_error();
  test_read_string();
  test_read_eof_by_page();
  test_read_eof_by_byte();
  test_read_chunks();
  test_read_empty();
  test_error_cursor();
  return 0;
}
