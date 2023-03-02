// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

typedef struct {
  int             n_base;
  int             n_prefix;
  int             n_statement;
  int             n_end;
  const SerdNode* graph;
} ReaderTest;

static SerdStatus
test_base_sink(void* handle, const SerdNode* uri)
{
  (void)uri;

  ReaderTest* rt = (ReaderTest*)handle;
  ++rt->n_base;
  return SERD_SUCCESS;
}

static SerdStatus
test_prefix_sink(void* handle, const SerdNode* name, const SerdNode* uri)
{
  (void)name;
  (void)uri;

  ReaderTest* rt = (ReaderTest*)handle;
  ++rt->n_prefix;
  return SERD_SUCCESS;
}

static SerdStatus
test_statement_sink(void*              handle,
                    SerdStatementFlags flags,
                    const SerdNode*    graph,
                    const SerdNode*    subject,
                    const SerdNode*    predicate,
                    const SerdNode*    object,
                    const SerdNode*    object_datatype,
                    const SerdNode*    object_lang)
{
  (void)flags;
  (void)subject;
  (void)predicate;
  (void)object;
  (void)object_datatype;
  (void)object_lang;

  ReaderTest* rt = (ReaderTest*)handle;
  ++rt->n_statement;
  rt->graph = graph;
  return SERD_SUCCESS;
}

static SerdStatus
test_end_sink(void* handle, const SerdNode* node)
{
  (void)node;

  ReaderTest* rt = (ReaderTest*)handle;
  ++rt->n_end;
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
test_read_chunks(const char* const path)
{
  static const char null = 0;

  FILE* const f = fopen(path, "w+b");

  // Write two statements separated by null characters
  fprintf(f, "@base <http://example.org/base/> .\n");
  fprintf(f, "@prefix eg: <http://example.org/> .\n");
  fprintf(f, "eg:s eg:p1 eg:o1 ;\n");
  fprintf(f, "     eg:p2 eg:o2 .\n");
  fwrite(&null, sizeof(null), 1, f);
  fprintf(f, "eg:s eg:p [ eg:sp eg:so ] .\n");
  fwrite(&null, sizeof(null), 1, f);
  fseek(f, 0, SEEK_SET);

  ReaderTest* const rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
  SerdReader* const reader = serd_reader_new(SERD_TURTLE,
                                             rt,
                                             free,
                                             test_base_sink,
                                             test_prefix_sink,
                                             test_statement_sink,
                                             test_end_sink);

  assert(reader);
  assert(serd_reader_get_handle(reader) == rt);
  assert(f);

  SerdStatus st = serd_reader_start_stream(reader, f, NULL, false);
  assert(st == SERD_SUCCESS);

  // Read base
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt->n_base == 1);
  assert(rt->n_prefix == 0);
  assert(rt->n_statement == 0);
  assert(rt->n_end == 0);

  // Read prefix
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt->n_base == 1);
  assert(rt->n_prefix == 1);
  assert(rt->n_statement == 0);
  assert(rt->n_end == 0);

  // Read first two statements
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt->n_base == 1);
  assert(rt->n_prefix == 1);
  assert(rt->n_statement == 2);
  assert(rt->n_end == 0);

  // Read terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt->n_base == 1);
  assert(rt->n_prefix == 1);
  assert(rt->n_statement == 2);
  assert(rt->n_end == 0);

  // Read statements after null terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_SUCCESS);
  assert(rt->n_base == 1);
  assert(rt->n_prefix == 1);
  assert(rt->n_statement == 4);
  assert(rt->n_end == 1);

  // Read terminator
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt->n_base == 1);
  assert(rt->n_prefix == 1);
  assert(rt->n_statement == 4);
  assert(rt->n_end == 1);

  // EOF
  st = serd_reader_read_chunk(reader);
  assert(st == SERD_FAILURE);
  assert(rt->n_base == 1);
  assert(rt->n_prefix == 1);
  assert(rt->n_statement == 4);
  assert(rt->n_end == 1);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  serd_reader_free(reader);
  fclose(f);
  remove(path);
}

static void
test_read_string(void)
{
  ReaderTest* rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
  SerdReader* reader = serd_reader_new(SERD_TURTLE,
                                       rt,
                                       free,
                                       test_base_sink,
                                       test_prefix_sink,
                                       test_statement_sink,
                                       test_end_sink);

  assert(reader);
  assert(serd_reader_get_handle(reader) == rt);

  // Test reading a string that ends exactly at the end of input (no newline)
  const SerdStatus st = serd_reader_read_string(
    reader,
    USTR("<http://example.org/s> <http://example.org/p> "
         "<http://example.org/o> ."));

  assert(!st);
  assert(rt->n_base == 0);
  assert(rt->n_prefix == 0);
  assert(rt->n_statement == 1);
  assert(rt->n_end == 0);

  serd_reader_free(reader);
}

static void
test_writer(const char* const path)
{
  FILE*    fd  = fopen(path, "wb");
  SerdEnv* env = serd_env_new(NULL);
  assert(fd);

  SerdWriter* writer =
    serd_writer_new(SERD_TURTLE, (SerdStyle)0, env, NULL, serd_file_sink, fd);
  assert(writer);

  serd_writer_chop_blank_prefix(writer, USTR("tmp"));
  serd_writer_chop_blank_prefix(writer, NULL);

  const SerdNode lit = serd_node_from_string(SERD_LITERAL, USTR("hello"));

  assert(serd_writer_set_base_uri(writer, &lit));
  assert(serd_writer_set_prefix(writer, &lit, &lit));
  assert(serd_writer_end_anon(writer, NULL));
  assert(serd_writer_get_env(writer) == env);

  uint8_t  buf[] = {0x80, 0, 0, 0, 0};
  SerdNode s     = serd_node_from_string(SERD_URI, USTR(""));
  SerdNode p = serd_node_from_string(SERD_URI, USTR("http://example.org/pred"));
  SerdNode o = serd_node_from_string(SERD_LITERAL, buf);

  // Write 3 invalid statements (should write nothing)
  const SerdNode* junk[][5] = {{&s, &p, &SERD_NODE_NULL, NULL, NULL},
                               {&s, &SERD_NODE_NULL, &o, NULL, NULL},
                               {&SERD_NODE_NULL, &p, &o, NULL, NULL},
                               {&s, &o, &o, NULL, NULL},
                               {&o, &p, &o, NULL, NULL},
                               {&s, &p, &SERD_NODE_NULL, NULL, NULL}};
  for (size_t i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 5); ++i) {
    assert(serd_writer_write_statement(writer,
                                       0,
                                       NULL,
                                       junk[i][0],
                                       junk[i][1],
                                       junk[i][2],
                                       junk[i][3],
                                       junk[i][4]));
  }

  const SerdNode  t         = serd_node_from_string(SERD_URI, USTR("urn:Type"));
  const SerdNode  l         = serd_node_from_string(SERD_LITERAL, USTR("en"));
  const SerdNode* good[][5] = {{&s, &p, &o, NULL, NULL},
                               {&s, &p, &o, &SERD_NODE_NULL, &SERD_NODE_NULL},
                               {&s, &p, &o, &t, NULL},
                               {&s, &p, &o, NULL, &l},
                               {&s, &p, &o, &t, &l},
                               {&s, &p, &o, &t, &SERD_NODE_NULL},
                               {&s, &p, &o, &SERD_NODE_NULL, &l},
                               {&s, &p, &o, NULL, &SERD_NODE_NULL},
                               {&s, &p, &o, &SERD_NODE_NULL, NULL},
                               {&s, &p, &o, &SERD_NODE_NULL, NULL}};
  for (size_t i = 0; i < sizeof(good) / (sizeof(SerdNode*) * 5); ++i) {
    assert(!serd_writer_write_statement(writer,
                                        0,
                                        NULL,
                                        good[i][0],
                                        good[i][1],
                                        good[i][2],
                                        good[i][3],
                                        good[i][4]));
  }

  // Write statements with bad UTF-8 (should be replaced)
  const uint8_t bad_str[] = {0xFF, 0x90, 'h', 'i', 0};
  SerdNode      bad_lit   = serd_node_from_string(SERD_LITERAL, bad_str);
  SerdNode      bad_uri   = serd_node_from_string(SERD_URI, bad_str);
  assert(!serd_writer_write_statement(
    writer, 0, NULL, &s, &p, &bad_lit, NULL, NULL));
  assert(!serd_writer_write_statement(
    writer, 0, NULL, &s, &p, &bad_uri, NULL, NULL));

  // Write 1 valid statement
  o = serd_node_from_string(SERD_LITERAL, USTR("hello"));
  assert(!serd_writer_write_statement(writer, 0, NULL, &s, &p, &o, NULL, NULL));

  serd_writer_free(writer);

  // Test chunk sink
  SerdChunk chunk = {NULL, 0};
  writer          = serd_writer_new(
    SERD_TURTLE, (SerdStyle)0, env, NULL, serd_chunk_sink, &chunk);

  o = serd_node_from_string(SERD_URI, USTR("http://example.org/base"));
  assert(!serd_writer_set_base_uri(writer, &o));

  serd_writer_free(writer);
  uint8_t* out = serd_chunk_sink_finish(&chunk);

  assert(!strcmp((const char*)out, "@base <http://example.org/base> .\n"));
  serd_free(out);

  // Test writing empty node
  SerdNode nothing = serd_node_from_string(SERD_NOTHING, USTR(""));

  chunk.buf = NULL;
  chunk.len = 0;
  writer    = serd_writer_new(
    SERD_TURTLE, (SerdStyle)0, env, NULL, serd_chunk_sink, &chunk);

  assert(!serd_writer_write_statement(
    writer, 0, NULL, &s, &p, &nothing, NULL, NULL));

  assert(
    !strncmp((const char*)chunk.buf, "<>\n\t<http://example.org/pred> ", 30));

  serd_writer_free(writer);
  out = serd_chunk_sink_finish(&chunk);

  assert(!strcmp((const char*)out, "<>\n\t<http://example.org/pred>  .\n"));
  serd_free(out);

  serd_env_free(env);
  fclose(fd);
}

static void
test_reader(const char* path)
{
  ReaderTest* rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
  SerdReader* reader = serd_reader_new(SERD_TURTLE,
                                       rt,
                                       free,
                                       test_base_sink,
                                       test_prefix_sink,
                                       test_statement_sink,
                                       test_end_sink);

  assert(reader);
  assert(serd_reader_get_handle(reader) == rt);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  SerdNode g = serd_node_from_string(SERD_URI, USTR("http://example.org/"));
  serd_reader_set_default_graph(reader, &g);
  serd_reader_add_blank_prefix(reader, USTR("tmp"));

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnonnull"
#endif
  serd_reader_add_blank_prefix(reader, NULL);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

  assert(serd_reader_read_file(reader, USTR("http://notafile")));
  assert(serd_reader_read_file(reader, USTR("file:///better/not/exist")));
  assert(serd_reader_read_file(reader, USTR("file://")));

  const SerdStatus st = serd_reader_read_file(reader, USTR(path));
  assert(!st);
  assert(rt->n_base == 0);
  assert(rt->n_prefix == 0);
  assert(rt->n_statement == 13);
  assert(rt->n_end == 0);
  assert(rt->graph && rt->graph->buf &&
         !strcmp((const char*)rt->graph->buf, "http://example.org/"));

  assert(serd_reader_read_string(reader, USTR("This isn't Turtle at all.")));

  // A read of a big page hits EOF then fails to read chunks immediately
  {
    FILE* const in = fopen(path, "rb");
    serd_reader_start_stream(reader, in, (const uint8_t*)"test", true);

    assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
    assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

    serd_reader_end_stream(reader);
    fclose(in);
  }

  // A byte-wise reader that hits EOF once then continues (like a socket)
  {
    size_t n_reads = 0;
    serd_reader_start_source_stream(reader,
                                    (SerdSource)eof_test_read,
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

  const char* const name     = "serd_test_reader_writer.ttl";
  const size_t      name_len = strlen(name);
  const size_t      path_len = tmp_len + 1 + name_len;
  char* const       path     = (char*)calloc(path_len + 1, 1);

  memcpy(path, tmp, tmp_len + 1);
  path[tmp_len] = '/';
  memcpy(path + tmp_len + 1, name, name_len + 1);

  test_read_chunks(path);
  test_read_string();

  test_writer(path);
  test_reader(path);

  assert(!remove(path));
  free(path);

  printf("Success\n");
  return 0;
}
