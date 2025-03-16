// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/env.h>
#include <serd/error.h>
#include <serd/node.h>
#include <serd/node_type.h>
#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/statement_flags.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/filesystem.h>
#include <zix/path.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  size_t n_written;
  size_t error_offset;
} ErrorContext;

typedef struct {
  int n_statement;
} ReaderTest;

static const char* const doc_string =
  "@base <http://drobilla.net/a/> .\n"
  "@prefix eg: <http://example.org/> .\n"
  "eg:g {\n"
  "<http://example.com/s> eg:p \"l\\n\\\"it\" ,\n"
  "  \"\"\"long\"\"\" ,\n"
  "  \"lang\"@en ;\n"
  "  eg:p <http://example.com/o> .\n"
  "}\n"
  "@prefix other: <http://example.org/other> .\n"
  "@base <http://drobilla.net/b/> .\n"
  "eg:𝚺\n"
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
  (void)graph;
  (void)subject;
  (void)predicate;
  (void)object;
  (void)object_datatype;
  (void)object_lang;

  ReaderTest* rt = (ReaderTest*)handle;
  ++rt->n_statement;
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
quiet_error_func(void* const handle, const SerdError* const e)
{
  (void)handle;
  (void)e;
  return SERD_SUCCESS;
}

static void
check_write_error_offset(SerdWorld* const world,
                         const SerdSyntax syntax,
                         const size_t     offset,
                         const SerdStatus expected_status)
{
  SerdEnv* const env = serd_env_new(NULL, NULL);
  assert(env);

  ErrorContext      ctx = {0U, offset};
  SerdWriter* const writer =
    serd_writer_new(world, syntax, 0U, env, faulty_sink, &ctx);
  assert(writer);

  SerdReader* const reader =
    serd_reader_new(world,
                    SERD_TRIG,
                    0U,
                    writer,
                    NULL,
                    (SerdBaseFunc)serd_writer_set_base_uri,
                    (SerdPrefixFunc)serd_writer_set_prefix,
                    (SerdStatementFunc)serd_writer_write_statement,
                    (SerdEndFunc)serd_writer_end_anon);
  assert(reader);

  SerdStatus rst = serd_reader_start_string(reader, doc_string);
  assert(!rst);

  if (!(rst = serd_reader_read_document(reader))) {
    rst = serd_reader_finish(reader);
  }

  const SerdStatus wst = serd_writer_finish(writer);
  const SerdStatus st  = rst ? rst : wst;

  serd_reader_free(reader);
  serd_writer_free(writer);
  serd_env_free(env);

  assert(st == expected_status);
}

static void
test_write_errors(void)
{
  // Syntax-keyed array of output document sizes
  static const size_t max_offsets[] = {0, 450, 1920, 2012, 464};

  SerdWorld* const world = serd_world_new(NULL);
  assert(world);
  serd_world_set_error_func(world, quiet_error_func, NULL);

  for (unsigned s = 1; s <= (unsigned)SERD_TRIG; ++s) {
    const SerdSyntax syntax = (SerdSyntax)s;

    // Check successfully writing with enough space
    check_write_error_offset(world, syntax, max_offsets[s], SERD_SUCCESS);

    // Check write error at every offset in the output
    for (size_t o = 0; o < max_offsets[s]; ++o) {
      check_write_error_offset(world, syntax, o, SERD_BAD_WRITE);
    }
  }

  serd_world_free(world);
}

static void
test_writer(const char* const path)
{
  FILE* const fd = fopen(path, "wb");
  assert(fd);

  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env   = serd_env_new(NULL, NULL);
  assert(world);
  assert(env);

  SerdWriter* writer = serd_writer_new(
    world, SERD_TURTLE, SERD_WRITE_LAX, env, serd_file_sink, fd);
  assert(writer);

  serd_writer_chop_blank_prefix(writer, "tmp");
  serd_writer_chop_blank_prefix(writer, NULL);

  const SerdNode lit = serd_node_from_string(SERD_LITERAL, "hello");

  assert(serd_writer_set_base_uri(writer, &lit));
  assert(serd_writer_set_prefix(writer, &lit, &lit));
  assert(serd_writer_end_anon(writer, NULL));
  assert(serd_writer_env(writer) == env);

  const uint8_t buf[] = {0xEF, 0xBF, 0xBD, 0};

  const SerdNode s = serd_node_from_string(SERD_URI, "");
  const SerdNode p = serd_node_from_string(SERD_URI, "http://example.org/pred");
  const SerdNode o = serd_node_from_string(SERD_LITERAL, (const char*)buf);
  const SerdNode t = serd_node_from_string(SERD_URI, "urn:Type");
  const SerdNode l = serd_node_from_string(SERD_LITERAL, "en");

  // Attempt to write invalid statements (should write nothing)
  const SerdNode* junk[][5] = {{&s, &p, &SERD_NODE_NULL, NULL, NULL},
                               {&s, &SERD_NODE_NULL, &o, NULL, NULL},
                               {&SERD_NODE_NULL, &p, &o, NULL, NULL},
                               {&s, &o, &o, NULL, NULL},
                               {&s, &o, &o, &t, &l},
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

  // Write some valid statements
  const SerdNode* good[][5] = {{&s, &p, &o, NULL, NULL},
                               {&s, &p, &lit, NULL, NULL},
                               {&s, &p, &o, &SERD_NODE_NULL, &SERD_NODE_NULL},
                               {&s, &p, &o, &t, NULL},
                               {&s, &p, &o, NULL, &l},
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

  static const uint8_t bad_uri_buf[]   = {'f', 't', 'p', ':', 0xFF, 0x90, 0};
  static const uint8_t bad_short_buf[] = {0xFF, 0x90, 'h', 'i', 0};
  static const uint8_t bad_long_buf[]  = {'e', '\n', 'r', 0xFF, 0x90, 0};

  // Write statements with bad UTF-8 (should be replaced)
  SerdNode bad_uri = serd_node_from_string(SERD_URI, (const char*)bad_uri_buf);
  SerdNode bad_short_lit =
    serd_node_from_string(SERD_LITERAL, (const char*)bad_short_buf);
  SerdNode bad_long_lit =
    serd_node_from_string(SERD_LITERAL, (const char*)bad_long_buf);
  assert(!serd_writer_write_statement(
    writer, 0, NULL, &s, &p, &bad_uri, NULL, NULL));
  assert(!serd_writer_write_statement(
    writer, 0, NULL, &s, &p, &bad_short_lit, NULL, NULL));
  assert(!serd_writer_write_statement(
    writer, 0, NULL, &s, &p, &bad_long_lit, NULL, NULL));

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
  assert(!fclose(fd));
}

static void
test_reader(const char* const path)
{
  SerdWorld* const  world = serd_world_new(NULL);
  ReaderTest* const rt    = (ReaderTest*)calloc(1, sizeof(ReaderTest));
  assert(world);
  assert(rt);

  SerdReader* const reader = serd_reader_new(
    world, SERD_TURTLE, 0U, rt, free, NULL, NULL, test_statement_sink, NULL);
  assert(reader);
  assert(serd_reader_handle(reader) == rt);

  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_document(reader) == SERD_FAILURE);

  serd_reader_add_blank_prefix(reader, "tmp");

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnonnull"
#endif
  serd_reader_add_blank_prefix(reader, NULL);
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

  assert(serd_reader_start_file(reader, "http://notafile", false));
  assert(serd_reader_start_file(reader, "file://invalid", false));
  assert(serd_reader_start_file(reader, "file:///nonexistant", false));

  assert(!serd_reader_start_file(reader, path, true));
  assert(!serd_reader_read_document(reader));
  assert(rt->n_statement == 13);
  assert(!serd_reader_finish(reader));

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

  char* const path = zix_path_join(NULL, dir, "serd_test_reader.ttl");
  assert(path);

  test_write_errors();

  test_writer(path);
  test_reader(path);

  assert(!zix_remove(path));
  assert(!zix_remove(dir));

  zix_free(NULL, path);
  zix_free(NULL, dir);
  zix_free(NULL, path_pattern);
  zix_free(NULL, temp);

  return 0;
}
