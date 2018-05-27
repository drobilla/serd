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

#include "console.h"

#include "serd/serd.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERDI_ERROR(msg) fprintf(stderr, "serdi: " msg)
#define SERDI_ERRORF(fmt, ...) fprintf(stderr, "serdi: " fmt, __VA_ARGS__)

#define MAX_SINKS 8

typedef struct {
  const char*       base_uri_string;
  const char*       out_filename;
  char* const*      inputs;
  intptr_t          n_inputs;
  SerdSyntaxOptions input_options;
  size_t            block_size;
  size_t            stack_size;
  bool              validate;
  bool              quiet;
} Options;

static SerdStatus
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Validate RDF data against RDFS and OWL schemas.\n"
    "INPUT can be a local filename, or \"-\" to read from standard input.\n\n"
    "  -B BASE_URI   Base URI or path for resolving relative references.\n"
    "  -I SYNTAX     Input syntax (turtle/ntriples/trig/nquads),\n"
    "                or option (lax/variables/verbatim).\n"
    "  -O SYNTAX     Output syntax (empty/turtle/ntriples/nquads),\n"
    "                or option (ascii/expanded/verbatim/terse/lax).\n"
    "  -V            Display version information and exit.\n"
    "  -W CHECKS     Validate with checks matching CHECKS.\n"
    "  -b BYTES      I/O block size.\n"
    "  -h            Display this help and exit.\n"
    "  -k BYTES      Parser stack size.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
  fprintf(os, "%s", description);
  return error ? SERD_ERR_BAD_ARG : SERD_SUCCESS;
}

static SerdStatus
consume_source(SerdWorld* const      world,
               const Options         opts,
               SerdSyntax            syntax,
               SerdEnv* const        env,
               const SerdSink* const sink,
               SerdInputStream       input)
{
  if (!input.stream) {
    return SERD_ERR_UNKNOWN;
  }

  SerdStatus        st     = SERD_SUCCESS;
  SerdReader* const reader = serd_reader_new(
    world, syntax, opts.input_options.flags, env, sink, opts.stack_size);

  if (!(st = serd_reader_start(reader, &input, NULL, opts.block_size))) {
    st = serd_reader_read_document(reader);
  }

  serd_reader_free(reader);
  serd_close_input(&input);
  return st;
}

static SerdStatus
read_file(SerdWorld* const      world,
          const Options         opts,
          SerdEnv* const        env,
          const SerdSink* const sink,
          const char* const     filename)
{
  SerdSyntax syntax = opts.input_options.syntax;
  syntax            = syntax ? syntax : serd_guess_syntax(filename);
  syntax            = syntax ? syntax : SERD_TRIG;

  SerdStatus st = SERD_SUCCESS;
  if (!opts.base_uri_string && strcmp(filename, "-")) {
    if ((st = serd_set_base_uri_from_path(env, filename))) {
      SERDI_ERRORF("failed to determine base URI from path %s\n", filename);
      return st;
    }
  }

  return consume_source(
    world,
    opts,
    serd_choose_syntax(world, opts.input_options, filename, SERD_TRIG),
    env,
    sink,
    serd_open_tool_input(filename));
}

static SerdStatus
parse_option(OptionIter* iter, Options* const opts)
{
  const char  opt      = iter->argv[iter->a][iter->f];
  const char* argument = NULL;

  switch (opt) {
  case 'B':
    return serd_get_argument(iter, &opts->base_uri_string);

  case 'I':
    return serd_parse_input_argument(iter, &opts->input_options);

  case 'W':
  case 'X':
    // Just enable validation and skip the pattern, checks are parsed later
    opts->validate = true;
    return serd_get_argument(iter, &argument);

  case 'b':
    return serd_get_size_argument(iter, &opts->block_size);

  case 'h':
    print_usage(iter->argv[0], false);
    return SERD_FAILURE;

  case 'k':
    return serd_get_size_argument(iter, &opts->stack_size);

  case 'q':
    opts->quiet = true;
    return serd_option_iter_advance(iter);

  case 'v':
    return serd_print_version("serdi");

  case 'w':
    return serd_get_argument(iter, &opts->out_filename);

  default:
    break;
  }

  SERDI_ERRORF("invalid option -- '%c'\n", opt);
  return print_usage(iter->argv[0], true);
}

typedef struct {
  size_t    n_sinks;
  SerdSink* sinks[MAX_SINKS];
} Pipeline;

static SerdSink*
push_sink(Pipeline* const pipeline, SerdSink* const sink)
{
  if (sink) {
    pipeline->sinks[pipeline->n_sinks++] = sink;
  }

  return sink;
}

static void
free_pipeline(Pipeline* const pipeline)
{
  for (size_t i = 0u; i < pipeline->n_sinks; ++i) {
    serd_sink_free(pipeline->sinks[i]);
  }
}

static SerdEnv*
build_env(Options opts)
{
  char* const* const inputs   = opts.inputs;
  const intptr_t     n_inputs = opts.n_inputs;

  if (!opts.base_uri_string && n_inputs == 1) {
    // Choose base URI from the single input path
    char* const input_path = serd_canonical_path(inputs[0]);

    SerdNode* base = input_path ? serd_new_file_uri(SERD_STRING(input_path),
                                                    SERD_EMPTY_STRING())
                                : NULL;
    if (!base) {
      SERDI_ERRORF("unable to determine base URI from path %s\n", inputs[0]);
    }

    SerdEnv* const env =
      serd_env_new(base ? serd_node_string_view(base) : SERD_EMPTY_STRING());
    serd_free(input_path);
    serd_node_free(base);
    return env;
  }

  return serd_env_new(opts.base_uri_string ? SERD_STRING(opts.base_uri_string)
                                           : SERD_EMPTY_STRING());
}

static SerdModel*
build_model(SerdWorld* const world, const Options opts, const bool with_graphs)
{
  SerdModel* const model = serd_model_new(
    world,
    with_graphs ? SERD_ORDER_GSPO : SERD_ORDER_SPO,
    (with_graphs * (unsigned)SERD_STORE_GRAPHS) | SERD_STORE_CARETS);

  if (with_graphs) {
    serd_model_add_index(model, SERD_ORDER_GSPO);
  }

  if (opts.validate) {
    serd_model_add_index(model, SERD_ORDER_POS);
  }

  serd_model_add_index(model, SERD_ORDER_OPS);
  if (with_graphs) {
    serd_model_add_index(model, SERD_ORDER_GOPS);
  }

  return model;
}

static bool
input_has_graphs(const Options opts)
{
  if (opts.input_options.syntax) {
    return serd_syntax_has_graphs(opts.input_options.syntax);
  }

  for (intptr_t i = 0u; i < opts.n_inputs; ++i) {
    if (serd_syntax_has_graphs(serd_guess_syntax(opts.inputs[i]))) {
      return true;
    }
  }

  return false;
}

static SerdStatus
read_inputs(SerdWorld*            world,
            const Options         opts,
            SerdEnv*              env,
            const SerdSink* const sink)
{
  SerdStatus st = SERD_SUCCESS;

  size_t prefix_len = 0;
  char*  prefix     = NULL;
  if (opts.n_inputs > 1) {
    prefix_len = 32; // FIXME
    prefix     = (char*)calloc(1, prefix_len);
  }

  for (intptr_t i = 0; !st && i < opts.n_inputs; ++i) {
    if (opts.n_inputs > 1) {
      snprintf(prefix, prefix_len, "f%" PRIdPTR, i);
    }

    st = read_file(world, opts, env, sink, opts.inputs[i]);
  }

  free(prefix);
  return st;
}

static SerdStatus
run(Options opts, int argc, char** argv)
{
  const bool with_graphs = input_has_graphs(opts);

  SerdOutputStream out = serd_open_tool_output(opts.out_filename);
  if (!out.stream) {
    perror("serdi: error opening output file");
    return SERD_ERR_UNKNOWN;
  }

  SerdWorld* const world    = serd_world_new();
  SerdEnv* const   env      = build_env(opts);
  SerdModel*       model    = NULL;
  Pipeline         pipeline = {0u, {NULL}};
  const SerdSink*  target   = NULL;

  model  = build_model(world, opts, with_graphs);
  target = push_sink(&pipeline, serd_inserter_new(model, NULL));

  const SerdSink* const sink = target;
  if (!sink) {
    SERDI_ERROR("failed to construct data pipeline, aborting\n");
    return SERD_ERR_BAD_ARG;
  }

  if (opts.quiet) {
    serd_world_set_log_func(world, serd_quiet_error_func, NULL);
  }

  SerdStatus st = read_inputs(world, opts, env, sink);

  if (st <= SERD_FAILURE) { // FIXME: ?
    SerdValidator* const validator = serd_validator_new(world);

    for (int i = 1; i < argc; ++i) {
      if (argv[i][0] == '-') {
        if (argv[i][1] == 'W') {
          serd_validator_enable_checks(validator, argv[++i]);
        } else if (argv[i][1] == 'X') {
          serd_validator_disable_checks(validator, argv[++i]);
        }
      }
    }

    st = serd_validate_model(validator, model, NULL);

    serd_validator_free(validator);
  }

  free_pipeline(&pipeline);
  serd_model_free(model);
  serd_env_free(env);
  serd_world_free(world);

  if (serd_close_output(&out)) {
    perror("serdi: write error");
    st = SERD_ERR_UNKNOWN;
  }

  return st;
}

int
main(int argc, char** argv)
{
  Options opts = {NULL,
                  NULL,
                  NULL,
                  0,
                  {SERD_SYNTAX_EMPTY, 0u, false},
                  4096u,
                  4194304u,
                  false,
                  false};

  // Parse all command line options (which must precede inputs)
  SerdStatus st   = SERD_SUCCESS;
  OptionIter iter = {argv, argc, 1, 1};
  while (!serd_option_iter_is_end(iter)) {
    if ((st = parse_option(&iter, &opts))) {
      return (st > SERD_FAILURE);
    }
  }

  // Every argument past the last option is an input
  opts.inputs   = argv + iter.a;
  opts.n_inputs = argc - iter.a;
  if (opts.n_inputs == 0) {
    SERDI_ERROR("missing input\n");
    print_usage(argv[0], true);
    return EXIT_FAILURE;
  }

  if (opts.n_inputs == 1) {
    opts.input_options.flags |= SERD_READ_GLOBAL;
  }

  st = st ? st : run(opts, argc, argv);

  return (st <= SERD_FAILURE) ? 0 : (int)st;
}
