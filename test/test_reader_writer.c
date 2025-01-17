// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/serd.h>

#ifdef _WIN32
#  include <windows.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USTR(s) ((const uint8_t*)(s))

typedef struct {
  size_t n_written;
  size_t error_offset;
} ErrorContext;

typedef struct {
  int             n_statement;
  const SerdNode* graph;
} ReaderTest;

static const char* const doc_string =
  "@base <http://drobilla.net/> .\n"
  "@prefix eg: <http://example.org/> .\n"
  "eg:g {\n"
  "<http://example.com/s> eg:p \"l\\n\\\"it\" ,\n"
  "  \"\"\"long\"\"\" ,\n"
  "  \"lang\"@en ;\n"
  "  eg:p <http://example.com/o> .\n"
  "}\n"
  "@prefix other: <http://example.org/other> .\n"
  "@base <http://drobilla.net/> .\n"
  "eg:s\n"
  "  <http://example.org/p> [\n"
  "    eg:p 3.0 ,\n"
  "      4 ,\n"
  "      \"lit\" ,\n"
  "      _:n42 ,\n"
  "      \"t\"^^eg:T\n"
  "  ] ;\n"
  "  eg:p () ;\n"
  "  eg:p\\!q (\"s\" 1 2.0 \"l\"@en eg:o) .\n"
  "[] eg:p eg:o .\n"
  "[ eg:p eg:o ] eg:q eg:r .\n"
  "( eg:o ) eg:t eg:u .\n";

static SerdStatus
test_statement_sink(void* const              handle,
                    const SerdStatementFlags flags,
                    const SerdNode* const    graph,
                    const SerdNode* const    subject,
                    const SerdNode* const    predicate,
                    const SerdNode* const    object,
                    const SerdNode* const    object_datatype,
                    const SerdNode* const    object_lang)
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

static size_t
faulty_sink(const void* const buf, const size_t len, void* const stream)
{
  (void)buf;
  (void)len;

  ErrorContext* const ctx           = (ErrorContext*)stream;
  const size_t        new_n_written = ctx->n_written + len;
  if (new_n_written >= ctx->error_offset) {
    errno = EINVAL;
    return 0U;
  }

  ctx->n_written += len;
  errno = 0;
  return len;
}

static SerdStatus
quiet_error_sink(void* const handle, const SerdError* const e)
{
  (void)handle;
  (void)e;
  return SERD_SUCCESS;
}

static void
test_write_errors(void)
{
  ErrorContext    ctx   = {0U, 0U};
  const SerdStyle style = (SerdStyle)(SERD_STYLE_STRICT | SERD_STYLE_CURIED);

  const size_t max_offsets[] = {0, 462, 1911, 2003, 462};

  // Test errors at different offsets to hit different code paths
  for (unsigned s = 1; s <= (unsigned)SERD_TRIG; ++s) {
    const SerdSyntax syntax = (SerdSyntax)s;
    for (size_t o = 0; o < max_offsets[s]; ++o) {
      ctx.n_written    = 0;
      ctx.error_offset = o;

      SerdEnv* const    env = serd_env_new(NULL);
      SerdWriter* const writer =
        serd_writer_new(syntax, style, env, NULL, faulty_sink, &ctx);

      SerdReader* const reader =
        serd_reader_new(SERD_TRIG,
                        writer,
                        NULL,
                        (SerdBaseSink)serd_writer_set_base_uri,
                        (SerdPrefixSink)serd_writer_set_prefix,
                        (SerdStatementSink)serd_writer_write_statement,
                        (SerdEndSink)serd_writer_end_anon);

      serd_reader_set_error_sink(reader, quiet_error_sink, NULL);
      serd_writer_set_error_sink(writer, quiet_error_sink, NULL);

      const SerdStatus st = serd_reader_read_string(reader, USTR(doc_string));
      assert(st == SERD_ERR_BAD_WRITE);

      serd_reader_free(reader);
      serd_writer_free(writer);
      serd_env_free(env);
    }
  }
}

static void
test_writer(const char* const path)
{
  FILE* const fd = fopen(path, "wb");
  assert(fd);

  SerdEnv* const env = serd_env_new(NULL);
  assert(env);

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

  const uint8_t buf[] = {0x80, 0, 0, 0, 0};

  SerdNode s = serd_node_from_string(SERD_URI, USTR(""));
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
  assert(!fclose(fd));
}

static void
test_reader(const char* const path)
{
  ReaderTest* rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
  SerdReader* reader = serd_reader_new(
    SERD_TURTLE, rt, free, NULL, NULL, test_statement_sink, NULL);

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
  assert(rt->n_statement == 13);
  assert(rt->graph && rt->graph->buf &&
         !strcmp((const char*)rt->graph->buf, "http://example.org/"));

  assert(serd_reader_read_string(reader, USTR("This isn't Turtle at all.")));

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

  const char* const ttl_name     = "serd_test_reader_writer.ttl";
  const size_t      ttl_name_len = strlen(ttl_name);
  const size_t      path_len     = tmp_len + 1 + ttl_name_len;
  char* const       path         = (char*)calloc(path_len + 1, 1);

  memcpy(path, tmp, tmp_len + 1);
  path[tmp_len] = '/';
  memcpy(path + tmp_len + 1, ttl_name, ttl_name_len + 1);

  test_write_errors();

  test_writer(path);
  test_reader(path);

  assert(!remove(path));
  free(path);

  return 0;
}
