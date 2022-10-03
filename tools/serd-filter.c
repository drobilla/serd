// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/attributes.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/filter.h"
#include "serd/input_stream.h"
#include "serd/log.h"
#include "serd/memory.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Application (after parsing command-line arguments) */

// All options
typedef struct {
  SerdCommonOptions common;
  const char*       pattern;
  const char*       pattern_file;
  char* const*      inputs;
  intptr_t          n_inputs;
  bool              invert;
} Options;

// A single statement pattern
typedef struct {
  SerdNode* s;
  SerdNode* p;
  SerdNode* o;
  SerdNode* g;
} FilterPattern;

// Context for the pattern event callback
typedef struct {
  SerdAllocator* allocator;
  FilterPattern  pattern;
} PatternEventContext;

// Handler for events read from a pattern
static SerdStatus
on_pattern_event(void* const handle, const SerdEvent* const event)
{
  PatternEventContext* const ctx       = (PatternEventContext*)handle;
  SerdAllocator* const       allocator = ctx->allocator;

  if (event->type == SERD_STATEMENT) {
    FilterPattern* const pat = &ctx->pattern;
    if (pat->s) {
      return SERD_BAD_PATTERN;
    }

    const SerdStatement* const statement = event->statement.statement;
    pat->s = serd_node_copy(allocator, serd_statement_subject(statement));
    pat->p = serd_node_copy(allocator, serd_statement_predicate(statement));
    pat->o = serd_node_copy(allocator, serd_statement_object(statement));
    pat->g = serd_node_copy(allocator, serd_statement_graph(statement));
  }

  return SERD_SUCCESS;
}

// Parse a pattern from some input and return a new filter for it
static SerdSink*
parse_pattern(SerdWorld* const       world,
              const SerdSink* const  sink,
              SerdInputStream* const in,
              const bool             inclusive)
{
  SerdAllocator* const allocator = serd_world_allocator(world);
  SerdEnv* const       env       = serd_env_new(world, serd_empty_string());
  PatternEventContext  ctx       = {allocator, {NULL, NULL, NULL, NULL}};

  SerdSink*   in_sink = serd_sink_new(world, &ctx, on_pattern_event, NULL);
  SerdReader* reader  = serd_reader_new(
    world, SERD_NQUADS, SERD_READ_VARIABLES, env, in_sink, 4096);

  const SerdNode* pattern_name =
    serd_nodes_get(serd_world_nodes(world), serd_a_string("pattern"));

  SerdStatus st = serd_reader_start(reader, in, pattern_name, 1);
  if (!st) {
    st = serd_reader_read_document(reader);
  }

  serd_close_input(in);
  serd_reader_free(reader);
  serd_env_free(env);
  serd_sink_free(in_sink);

  if (st) {
    serd_logf(world,
              SERD_LOG_LEVEL_ERROR,
              "failed to parse pattern (%s)",
              serd_strerror(st));
    return NULL;
  }

  SerdSink* filter = serd_filter_new(world,
                                     sink,
                                     ctx.pattern.s,
                                     ctx.pattern.p,
                                     ctx.pattern.o,
                                     ctx.pattern.g,
                                     inclusive);

  serd_node_free(allocator, ctx.pattern.s);
  serd_node_free(allocator, ctx.pattern.p);
  serd_node_free(allocator, ctx.pattern.o);
  serd_node_free(allocator, ctx.pattern.g);
  return filter;
}

SERD_LOG_FUNC(2, 3)
static SerdStatus
log_error(SerdWorld* const world, const char* const fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  const SerdLogField file = {"SERD_FILE", "serd-filter"};
  const SerdStatus   st =
    serd_vxlogf(world, SERD_LOG_LEVEL_ERROR, 1, &file, fmt, args);

  va_end(args);
  return st;
}

// Run the tool using the given options
static SerdStatus
run(Options opts)
{
  SerdTool app = {{NULL, NULL, NULL, NULL}, NULL, NULL, NULL};

  // Set up the writing environment
  SerdStatus st = SERD_SUCCESS;
  if ((st = serd_tool_setup(&app, "serd-filter", opts.common))) {
    serd_tool_cleanup(app);
    return st;
  }

  const SerdSink* const target = serd_writer_sink(app.writer);

  // Open the pattern input (either a string or filename)
  SerdInputStream pattern  = {NULL, NULL, NULL, NULL};
  const char*     position = opts.pattern;
  if (opts.pattern) {
    pattern = serd_open_input_string(&position);
  } else if (opts.pattern_file) {
    pattern = serd_open_input_file(opts.pattern_file);
  }

  if (!pattern.stream) {
    log_error(app.world, "failed to open pattern");
    return SERD_UNKNOWN_ERROR;
  }

  // Set up the output pipeline: filter -> writer
  SerdSink* const filter =
    parse_pattern(app.world, target, &pattern, !opts.invert);
  if (!filter) {
    log_error(app.world, "failed to set up filter");
    return SERD_UNKNOWN_ERROR;
  }

  serd_close_input(&pattern);

  // Read all the inputs, which drives the writer to emit the output
  if (!(st = serd_read_inputs(app.world,
                              opts.common,
                              app.env,
                              opts.n_inputs,
                              opts.inputs,
                              filter))) {
    st = serd_writer_finish(app.writer);
  }

  if (st) {
    log_error(app.world, "failed to read input (%s)", serd_strerror(st));
  }

  serd_sink_free(filter);

  const SerdStatus cst = serd_tool_cleanup(app);
  return st ? st : cst;
}

/* Command-line interface (before setting up serd) */

static int
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Search for statements matching PATTERN in each INPUT.\n"
    "INPUT can be a local filename, or \"-\" to read from standard input.\n\n"
    "  -B BASE_URI      Base URI or path for resolving relative references.\n"
    "  -I SYNTAX        Input syntax (turtle/ntriples/trig/nquads),\n"
    "                   or option (lax/variables/relative/global/generated).\n"
    "  -O SYNTAX        Output syntax (empty/turtle/ntriples/nquads),\n"
    "                   or option "
    "(ascii/contextual/expanded/verbatim/terse/lax).\n"
    "  -V               Display version information and exit.\n"
    "  -f PATTERN_FILE  Read pattern from PATTERN_FILE instead.\n"
    "  -h               Display this help and exit.\n"
    "  -k BYTES         Parser stack size.\n"
    "  -o FILENAME      Write output to FILENAME instead of stdout.\n"
    "  -v               Invert filter to select non-matching statements.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... PATTERN INPUT...\n", name);
  fprintf(os, "       %s [OPTION]... -f PATTERN_FILE INPUT...\n", name);
  fprintf(os, "\n%s", description);
  return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Parse the option pointed to by `iter`, and advance it to the next one
static SerdStatus
parse_option(OptionIter* const iter, Options* const opts)
{
#define ARG_ERRORF(fmt, ...) \
  fprintf(stderr, "%s: " fmt, iter->argv[0], __VA_ARGS__)

  SerdStatus st = serd_parse_common_option(iter, &opts->common);
  if (st != SERD_FAILURE) {
    return st;
  }

  const char opt = iter->argv[iter->a][iter->f];

  switch (opt) {
  case 'V':
    return serd_print_version("serd-filter");

  case 'f':
    return serd_get_argument(iter, &opts->pattern_file);

  case 'h':
    print_usage(iter->argv[0], false);
    return SERD_FAILURE;

  case 'v':
    opts->invert = true;
    return serd_option_iter_advance(iter);

  default:
    break;
  }

  ARG_ERRORF("invalid option -- '%c'\n", opt);
  return SERD_BAD_ARG;

#undef ARG_ERRORF
}

int
main(int argc, char** argv)
{
  char  default_input[]  = "-";
  char* default_inputs[] = {default_input};

  Options opts = {{"",
                   NULL,
                   4096U,
                   1048576U,
                   {SERD_SYNTAX_EMPTY, 0U, false},
                   {SERD_NQUADS, 0U, false}},
                  NULL,
                  NULL,
                  NULL,
                  0U,
                  false};

  // Parse all command line options (which must precede inputs)
  SerdStatus st   = SERD_SUCCESS;
  OptionIter iter = {argv, argc, 1, 1};
  while (!serd_option_iter_is_end(iter)) {
    if ((st = parse_option(&iter, &opts))) {
      return (st == SERD_FAILURE) ? 0 : print_usage(argv[0], true);
    }
  }

  // If -f isn't used, then the first positional argument is the pattern
  if (!opts.pattern_file) {
    opts.pattern = argv[iter.a++];
  }

  // Every argument past that is an input
  opts.inputs   = argv + iter.a;
  opts.n_inputs = argc - iter.a;
  if (opts.n_inputs == 0) {
    opts.n_inputs = 1;
    opts.inputs   = default_inputs;
  }

  // Don't add prefixes to blank node labels if there is only one input
  if (opts.n_inputs == 1) {
    opts.common.input.flags |= SERD_READ_GLOBAL;
  }

  return run(opts) > SERD_FAILURE;
}
