// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/env.h>
#include <serd/error.h>
#include <serd/event.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/filesystem.h>
#include <zix/path.h>
#include <zix/string_view.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
test_sink(void* const handle, const SerdEvent* const event)
{
  ReaderTest* const rt = (ReaderTest*)handle;

  assert(event->type == SERD_EVENT_STATEMENT);

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
  SerdEnv* const env = serd_env_new(NULL, zix_empty_string());
  assert(env);

  ErrorContext ctx = {0U, offset};

  SerdWriter* const writer =
    serd_writer_new(world, syntax, 0U, env, faulty_sink, &ctx);
  assert(writer);

  const SerdSink* const sink   = serd_writer_sink(writer);
  SerdReader* const     reader = serd_reader_new(world, SERD_TRIG, 0U, sink);
  assert(reader);

  SerdStatus rst =
    serd_reader_start_string(reader, doc_string, zix_empty_string());
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

  static const SerdLimits limits = {1024, 128};

  SerdWorld* const world = serd_world_new(NULL);
  assert(world);
  serd_world_set_limits(world, limits);
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
  SerdEnv* const   env   = serd_env_new(NULL, zix_empty_string());
  assert(world);
  assert(env);

  SerdWriter* writer = serd_writer_new(
    world, SERD_TURTLE, SERD_WRITE_LAX, env, serd_file_sink, fd);
  assert(writer);

  serd_writer_chop_blank_prefix(writer, "tmp");
  serd_writer_chop_blank_prefix(writer, NULL);

  const SerdSink* const sink = serd_writer_sink(writer);

  // Check invalid calls to basic sink methods
  assert(serd_sink_event(sink, serd_base_event(zix_string("rel"))));
  assert(serd_sink_event(
    sink, serd_prefix_event(zix_string("name"), zix_string("rel"))));
  assert(serd_sink_event(sink, serd_end_event(zix_string("whatever"))));

  static const uint8_t       bad_buf[]    = {0xEF, 0xBF, 0xBD, 0};
  static const ZixStringView bad_buf_view = {(const char*)bad_buf, 3};

  const SerdTokenView s   = {SERD_URI, zix_string("http://example.org")};
  const SerdTokenView p   = {SERD_URI, zix_string("http://example.org/pred")};
  const SerdTokenView bad = {SERD_LITERAL, bad_buf_view};

  // Write 3 invalid statements (should write nothing)
  const SerdTokenView junk[][3] = {{s, bad, bad}, {bad, p, bad}, {s, bad, p}};
  for (size_t i = 0; i < sizeof(junk) / (sizeof(SerdTokenView) * 3); ++i) {
    const SerdObjectView o = {
      junk[i][2].type, junk[i][2].string, 0U, serd_no_token()};

    assert(serd_sink_event(
      sink,
      serd_statement_event(0U, serd_triple_view(junk[i][0], junk[i][1], o))));
  }

  // Write some valid statements
  {
    const SerdTokenView urn_Type = {SERD_URI, zix_string("urn:Type")};
    const SerdTokenView en       = {SERD_LITERAL, zix_string("en")};

    const SerdObjectView o = {
      SERD_LITERAL, zix_string("o"), 0U, serd_no_token()};

    const SerdObjectView t = {
      SERD_LITERAL, zix_string("t"), SERD_HAS_DATATYPE, urn_Type};

    const SerdObjectView l = {
      SERD_LITERAL, zix_string("l"), SERD_HAS_LANGUAGE, en};

    const SerdObjectView good[3] = {o, t, l};

    for (size_t i = 0; i < sizeof(good) / sizeof(SerdObjectView); ++i) {
      assert(!serd_sink_event(
        sink, serd_statement_event(0U, serd_triple_view(s, p, good[i]))));
    }
  }

  static const uint8_t bad_uri_buf[]   = {'f', 't', 'p', ':', 0xFF, 0x90, 0};
  static const uint8_t bad_short_buf[] = {0xFF, 0x90, 'h', 'i', 0};
  static const uint8_t bad_long_buf[]  = {'e', '\n', 'r', 0xFF, 0x90, 0};

  // Write statements with bad UTF-8 (should be replaced)
  const ZixStringView  bad_uri_str   = zix_string((const char*)bad_uri_buf);
  const ZixStringView  bad_short_str = zix_string((const char*)bad_short_buf);
  const ZixStringView  bad_long_str  = zix_string((const char*)bad_long_buf);
  const SerdTokenView  meta          = {SERD_NOTHING, ZIX_STATIC_STRING("")};
  const SerdObjectView bad_uri       = {SERD_URI, bad_uri_str, 0U, meta};
  const SerdObjectView bad_short_lit = {SERD_LITERAL, bad_short_str, 0U, meta};
  const SerdObjectView bad_long_lit  = {
    SERD_LITERAL, bad_long_str, SERD_HAS_NEWLINE, meta};
  assert(!serd_sink_event(
    sink, serd_statement_event(0U, serd_triple_view(s, p, bad_uri))));
  assert(!serd_sink_event(
    sink, serd_statement_event(0U, serd_triple_view(s, p, bad_short_lit))));
  assert(!serd_sink_event(
    sink, serd_statement_event(0U, serd_triple_view(s, p, bad_long_lit))));

  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);
  assert(!fclose(fd));
}

static void
test_reader(const char* const path)
{
  SerdWorld* const world = serd_world_new(NULL);
  assert(world);

  ReaderTest     rt   = {0};
  const SerdSink sink = {&rt, test_sink};

  const SerdLimits limits = {1024, 0};
  serd_world_set_limits(world, limits);

  // Test that too little stack space fails gracefully
  const SerdLimits old_limits  = serd_world_limits(world);
  const SerdLimits tiny_limits = {32, 0};
  serd_world_set_limits(world, tiny_limits);
  assert(!serd_reader_new(world, SERD_TURTLE, 0U, &sink));

  // Restore limits and successfully create reader
  serd_world_set_limits(world, old_limits);
  SerdReader* const reader = serd_reader_new(world, SERD_TURTLE, 0U, &sink);
  assert(reader);

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
  assert(rt.n_statement == 6);
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
