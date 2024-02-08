// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/env.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
  MODE_SUCCESS,
  MODE_BAD_STREAM,
  MODE_BAD_CHAR,
} FailMode;

/// Input stream wrapper that simulates read failure at some offset
typedef struct {
  const SerdNode* base;
  FILE*           file;
  size_t          offset;
  size_t          error_offset;
  int             error_status;
  FailMode        mode;
} BadContext;

static size_t
bad_read(void* const  buf,
         const size_t size,
         const size_t nmemb,
         void* const  stream)
{
  assert(size == 1U);

  BadContext* const ctx = (BadContext*)stream;

  const size_t begin  = ctx->offset;
  const size_t end    = begin + nmemb;
  const bool   is_bad = (begin <= ctx->error_offset && end > ctx->error_offset);
  if (is_bad && ctx->mode == MODE_BAD_STREAM) {
    ctx->error_status = 1;
  }

  if (ctx->error_status) {
    return 0U;
  }

  const size_t n = fread(buf, size, nmemb, ctx->file);
  if (is_bad && ctx->mode != MODE_SUCCESS) {
    const size_t offset     = ctx->error_offset - begin;
    ((uint8_t*)buf)[offset] = (ctx->mode == MODE_BAD_CHAR ? 0xF8U : 0U);
  }

  ctx->offset = end;
  return n;
}

static int
bad_error(void* const stream)
{
  BadContext* const ctx = (BadContext*)stream;

  return ctx->error_status ? ctx->error_status : ferror(ctx->file);
}

static SerdStatus
run_offset(SerdReader* const reader,
           BadContext* const ctx,
           const size_t      error_offset)
{
  ctx->offset       = 0U;
  ctx->error_offset = error_offset;
  ctx->error_status = 0;

  SerdInputStream in = serd_open_input_stream(bad_read, bad_error, NULL, ctx);
  assert(!serd_reader_start(reader, &in, ctx->base, 1U));

  const SerdStatus st = serd_reader_read_document(reader);

  serd_reader_finish(reader);
  serd_close_input(&in);
  return st;
}

static int
check_status(const FailMode   mode,
             const SerdStatus st,
             const size_t     error_offset)
{
  if ((mode == MODE_SUCCESS && st) ||
      (mode == MODE_BAD_STREAM && st != SERD_BAD_STREAM) ||
      (mode == MODE_BAD_CHAR && st != SERD_BAD_TEXT && st != SERD_BAD_SYNTAX)) {
    fprintf(stderr, "error: Expected read error at offset %zu\n", error_offset);
    fprintf(stderr, "note: Actual status: %s\n", serd_strerror(st));
    return 1;
  }

  return 0;
}

static int
run(const ZixStringView filename, const SerdSyntax syntax, const FailMode mode)
{
  static const ZixStringView host = ZIX_STATIC_STRING("");

  FILE* const file = fopen(filename.data, "rb");
  if (!file) {
    fprintf(stderr, "error: Failed to open input \"%s\"\n", filename.data);
    return 1;
  }

  // Fast-forward to the end to find the file size
  fseek(file, 0, SEEK_END);
  const long file_end = ftell(file);
  if (file_end <= 0) {
    fprintf(stderr, "error: Empty input\n");
    return 2;
  }

  size_t start_offset = 0U;
  if (mode == MODE_BAD_CHAR) {
    // Find a start offset past a comment header if necessary
    // (Line comment parsing is permissive and outside the grammar)
    fseek(file, 0, SEEK_SET);
    int first = fgetc(file);
    if (first == '#') {
      do {
        ++start_offset;
        first = fgetc(file);
      } while (first > 0 && first != '\n' && first != '\r');
      ++start_offset;
    }
    ungetc(first, file);
  }

  // Create a simple world and context for running the reader
  SerdWorld* const world = serd_world_new(NULL);
  SerdNode* const  base  = serd_node_new(NULL, serd_a_file_uri(filename, host));
  SerdEnv* const   env   = serd_env_new(NULL, serd_node_string_view(base));
  BadContext       ctx   = {base, file, 0U, 0U, 0, MODE_SUCCESS};

  const SerdSink* const sink = serd_env_sink(env);
  SerdReader* const     reader =
    serd_reader_new(world, syntax, SERD_READ_VARIABLES, env, sink);

  SerdStatus   st        = SERD_SUCCESS;
  const size_t file_size = (size_t)file_end;

  // Check that a run without a read error succeeds
  fseek(file, 0, SEEK_SET);
  if (check_status(ctx.mode,
                   run_offset(reader, &ctx, (size_t)(file_end + 2)),
                   (size_t)(file_end + 2))) {
    return 3;
  }

  ctx.mode = mode;

  int rc = 0;
  for (size_t offset = start_offset; !rc && offset < file_size; ++offset) {
    fseek(file, 0, SEEK_SET);
    st = run_offset(reader, &ctx, offset);
    rc = check_status(ctx.mode, st, offset);
  }

  if (!rc) {
    fprintf(stderr, "Checked errors at every offset up to %zu\n", file_size);
  }

  serd_reader_free(reader);
  serd_env_free(env);
  serd_node_free(NULL, base);
  serd_world_free(world);
  return rc;
}

static int
print_usage(const char* const program, const int rc)
{
  fprintf(stderr,
          "Usage: %s [OPTIONS] FILE\n\n"
          "  -c  Simulate invalid UTF-8 bytes.\n"
          "  -s  Simulate stream errors.\n",
          program);
  return rc;
}

int
main(const int argc, char** const argv)
{
  if (argc < 2) {
    return print_usage(argv[0], 4);
  }

  FailMode mode = MODE_BAD_STREAM;
  int      a    = 1;
  for (; a < argc && argv[a][0] == '-'; ++a) {
    if (argv[a][0] == '-') {
      const char opt = argv[a][1];
      if (opt == 'c') {
        mode = MODE_BAD_CHAR;
      } else if (opt == 's') {
        mode = MODE_BAD_STREAM;
      } else {
        return print_usage(argv[0], 5);
      }
    }
  }

  const char* const filename = argv[a];
  const SerdSyntax  syntax   = serd_guess_syntax(filename);

  return run(zix_string(filename),
             syntax == SERD_SYNTAX_EMPTY ? SERD_TRIG : syntax,
             mode);
}
