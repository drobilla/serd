// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/canon.h"
#include "serd/env.h"
#include "serd/error.h"
#include "serd/input_stream.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/tee.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/string_view.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// All options
typedef struct {
  SerdCommonOptions common;
  const char*       root_uri;
  const char*       input_string;
  char* const*      inputs;
  intptr_t          n_inputs;
  bool              canonical;
  bool              quiet;
} Options;

static SerdStatus
quiet_error_func(void* const handle, const SerdError* const e)
{
  (void)handle;
  (void)e;
  return SERD_SUCCESS;
}

// Run the tool using the given options
static SerdStatus
run(const Options opts)
{
  SerdTool app = {{NULL, NULL, NULL}, NULL, NULL, NULL, NULL};

  // Set up the writing environment
  SerdStatus st = SERD_SUCCESS;
  if ((st = serd_tool_setup(&app, "serd-pipe", opts.common))) {
    (void)serd_tool_cleanup(app);
    return st;
  }

  serd_writer_set_root_uri(app.writer, zix_string(opts.root_uri));

  if (opts.quiet) {
    serd_world_set_error_func(app.world, quiet_error_func, NULL);
  }

  // Set up the output pipeline: [canon] -> writer
  const SerdSink* const target   = serd_writer_sink(app.writer);
  const SerdSink*       out_sink = target;
  SerdSink*             canon    = NULL;
  if (opts.canonical) {
    canon    = serd_canon_new(app.world, target, opts.common.input.flags);
    out_sink = canon;
  }

  SerdSink* const sink = serd_tee_new(NULL, serd_env_sink(app.env), out_sink);

  if (opts.input_string) {
    const char*     position = opts.input_string;
    SerdInputStream in       = serd_open_input_string(&position);

    st = serd_read_source(
      app.world,
      opts.common,
      app.env,
      serd_choose_syntax(&app, opts.common.input, NULL, SERD_TRIG),
      &in,
      "string",
      sink);

    serd_close_input(&in);
  }

  // Read all the inputs, which drives the writer to emit the output
  if (!st) {
    st = serd_read_inputs(&app, opts.common, opts.n_inputs, opts.inputs, sink);
  }

  serd_sink_free(sink);
  serd_sink_free(canon);

  const SerdStatus wst = serd_writer_finish(app.writer);
  const SerdStatus cst = serd_tool_cleanup(app);
  return st ? st : wst ? wst : cst;
}

/* Command-line interface (before setting up serd) */

static int
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Read and write RDF data.\n"
    "INPUT can be a local filename, or \"-\" to read from standard input.\n\n"
    "  -B URI     Resolve URIs against the given base URI or path.\n"
    "  -C         Convert literals to canonical form.\n"
    "  -I SYNTAX  Input syntax nquads/ntriples/trig/turtle, or option\n"
    "             decoded/generated/global/lax/relative/variables.\n"
    "  -O SYNTAX  Output syntax empty/nquads/ntriples/trig/turtle, or option\n"
    "             ascii/contextual/encoded/expanded/lax/terse/verbatim.\n"
    "  -R URI     Keep relative URIs within the given root URI.\n"
    "  -V         Display version information and exit.\n"
    "  -b BYTES   I/O block size.\n"
    "  -h         Display this help and exit.\n"
    "  -k BYTES   Parser stack size.\n"
    "  -o FILE    Write output to FILE instead of stdout.\n"
    "  -q         Suppress warning and error output.\n"
    "  -s STRING  Parse STRING as input.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... [INPUT]...\n", name);
  fprintf(os, "%s", description);
  return error;
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

  if (!strcmp(iter->argv[iter->a], "--help")) {
    print_usage(iter->argv[0], false);
    return SERD_FAILURE;
  }

  if (!strcmp(iter->argv[iter->a], "--version")) {
    return serd_print_version(iter->argv[0]);
  }

  const char opt = iter->argv[iter->a][iter->f];
  switch (opt) {
  case 'C':
    opts->canonical = true;
    return serd_option_iter_advance(iter);

  case 'R':
    return serd_get_argument(iter, &opts->root_uri);

  case 'V':
    return serd_print_version("serd-pipe");

  case 'h':
    print_usage(iter->argv[0], false);
    return SERD_FAILURE;

  case 'q':
    opts->quiet = true;
    return serd_option_iter_advance(iter);

  case 's':
    return serd_get_argument(iter, &opts->input_string);

  default:
    break;
  }

  ARG_ERRORF("invalid option -- '%c'\n", opt);
  return SERD_BAD_ARG;

#undef ARG_ERRORF
}

int
main(const int argc, char* const* const argv)
{
  char  default_input[]  = {'-', '\0'};
  char* default_inputs[] = {default_input};

  Options opts = {serd_default_options(), "", NULL, NULL, 0U, false, false};

  // Parse all command line options (which must precede inputs)
  SerdStatus st   = SERD_SUCCESS;
  OptionIter iter = {argv, argc, 1, 1};
  while (!serd_option_iter_is_end(iter)) {
    if ((st = parse_option(&iter, &opts))) {
      return (st == SERD_FAILURE) ? 0 : print_usage(argv[0], true);
    }
  }

  // Every argument past the last option is an input
  opts.inputs   = argv + iter.a;
  opts.n_inputs = argc - iter.a;
  if (opts.n_inputs + (bool)opts.input_string == 0) {
    opts.n_inputs = 1;
    opts.inputs   = default_inputs;
  }

  // Don't add prefixes to blank node labels if there is only one input
  if (opts.n_inputs + (bool)opts.input_string == 1) {
    opts.common.input.flags |= SERD_READ_GLOBAL;
  }

  return run(opts) > SERD_FAILURE;
}
