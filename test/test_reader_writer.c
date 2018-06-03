// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SerdStatus
test_sink(void*                handle,
          SerdStatementFlags   flags,
          const SerdStatement* statement)
{
  (void)flags;
  (void)statement;

  ++*(size_t*)handle;

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
test_read_chunks(void)
{
  SerdWorld*        world        = serd_world_new();
  size_t            n_statements = 0;
  FILE* const       f            = tmpfile();
  static const char null         = 0;
  SerdSink*         sink         = serd_sink_new(&n_statements, NULL);
  SerdReader*       reader = serd_reader_new(world, SERD_TURTLE, sink, 4096);

  assert(reader);
  assert(sink);
  assert(f);
  serd_sink_set_statement_func(sink, test_sink);

  SerdStatus st = serd_reader_start_stream(
    reader, (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, f, NULL, 1);
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

  serd_reader_free(reader);
  serd_sink_free(sink);
  fclose(f);
  serd_world_free(world);
}

static void
test_read_string(void)
{
  SerdWorld*  world        = serd_world_new();
  size_t      n_statements = 0;
  SerdSink*   sink         = serd_sink_new(&n_statements, NULL);
  SerdReader* reader       = serd_reader_new(world, SERD_TURTLE, sink, 4096);

  assert(reader);
  assert(sink);

  serd_sink_set_statement_func(sink, test_sink);

  // Test reading a string that ends exactly at the end of input (no newline)
  assert(
    !serd_reader_start_string(reader,
                              "<http://example.org/s> <http://example.org/p> "
                              "<http://example.org/o> .",
                              NULL));

  assert(!serd_reader_read_document(reader));
  assert(n_statements == 1);
  assert(!serd_reader_finish(reader));

  serd_reader_free(reader);
  serd_sink_free(sink);
  serd_world_free(world);
}

static void
test_writer(const char* const path)
{
  FILE*    fd  = fopen(path, "wb");
  SerdEnv* env = serd_env_new(serd_empty_string());
  assert(fd);

  SerdWorld* world = serd_world_new();

  SerdWriter* writer =
    serd_writer_new(world, SERD_TURTLE, 0, env, (SerdWriteFunc)fwrite, fd);
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

  serd_writer_set_base_uri(writer, base);

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
  SerdWorld*      world        = serd_world_new();
  size_t          n_statements = 0;
  SerdSink* const sink         = serd_sink_new(&n_statements, NULL);
  assert(sink);
  serd_sink_set_statement_func(sink, test_sink);

  // Test that too little stack space fails gracefully
  assert(!serd_reader_new(world, SERD_TURTLE, sink, 32));

  SerdReader* reader = serd_reader_new(world, SERD_TURTLE, sink, 4096);
  assert(reader);

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
  assert(n_statements == 6);
  serd_reader_finish(reader);

  // A read of a big page hits EOF then fails to read chunks immediately
  {
    FILE* temp = tmpfile();
    assert(temp);
    fprintf(temp, "_:s <http://example.org/p> _:o .\n");
    fflush(temp);
    fseek(temp, 0L, SEEK_SET);

    serd_reader_start_stream(reader,
                             (SerdReadFunc)fread,
                             (SerdStreamErrorFunc)ferror,
                             temp,
                             NULL,
                             4096);

    assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

    serd_reader_finish(reader);
    fclose(temp);
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
  test_read_chunks();
  test_read_string();

  const char* const path = "serd_test.ttl";
  test_writer(path);
  test_reader(path);

  printf("Success\n");
  return 0;
}
