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
  SerdNode* s;
  SerdNode* p;
  SerdNode* o;
  SerdNode* g;
} FilterPattern;

typedef struct {
  const char*       base_uri_string;
  const char*       input_string;
  const char*       in_pattern;
  const char*       out_pattern;
  const char*       add_prefix;
  const char*       chop_prefix;
  const char*       root_uri;
  const char*       out_filename;
  char* const*      inputs;
  intptr_t          n_inputs;
  SerdSyntaxOptions input_options;
  SerdSyntaxOptions output_options;
  size_t            block_size;
  size_t            stack_size;
  bool              no_inline;
  bool              validate;
  bool              use_model;
  bool              canonical;
  bool              quiet;
} Options;

static SerdStatus
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Read and write RDF syntax.\n"
    "Use - for INPUT to read from standard input.\n\n"
    "  -C           Convert literals to canonical form.\n"
    "  -F PATTERN   Filter out statements that match PATTERN.\n"
    "  -G PATTERN   Only include statements matching PATTERN.\n"
    "  -I BASE_URI  Input base URI.\n"
    "  -V CHECKS    Validate with checks matching CHECKS.\n"
    "  -b BYTES     I/O block size.\n"
    "  -c PREFIX    Chop PREFIX from matching blank node IDs.\n"
    "  -f           Fast and loose mode (possibly ugly output).\n"
    "  -h           Display this help and exit.\n"
    "  -i SYNTAX    Input syntax (turtle/ntriples/trig/nquads),\n"
    "               or flag (lax/variables/verbatim).\n"
    "  -k BYTES     Parser stack size.\n"
    "  -m           Build a model in memory before writing.\n"
    "  -o SYNTAX    Output syntax (empty/turtle/ntriples/nquads),\n"
    "               or flag (ascii/expanded/verbatim/terse/lax).\n"
    "  -p PREFIX    Add PREFIX to blank node IDs.\n"
    "  -q           Suppress all output except data.\n"
    "  -r ROOT_URI  Keep relative URIs within ROOT_URI.\n"
    "  -s STRING    Parse STRING as input.\n"
    "  -v           Display version information and exit.\n"
    "  -w FILENAME  Write output to FILENAME instead of stdout.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
  fprintf(os, "%s", description);
  return error ? SERD_ERR_BAD_ARG : SERD_SUCCESS;
}

static SerdStatus
on_filter_event(void* const handle, const SerdEvent* const event)
{
  if (event->type == SERD_STATEMENT) {
    FilterPattern* const pat = (FilterPattern*)handle;
    if (pat->s) {
      return SERD_ERR_INVALID;
    }

    const SerdStatement* const statement = event->statement.statement;
    pat->s = serd_node_copy(serd_statement_subject(statement));
    pat->p = serd_node_copy(serd_statement_predicate(statement));
    pat->o = serd_node_copy(serd_statement_object(statement));
    pat->g = serd_node_copy(serd_statement_graph(statement));
  }

  return SERD_SUCCESS;
}

static SerdSink*
parse_filter(SerdWorld* const      world,
             const SerdSink* const sink,
             const char* const     str,
             const bool            inclusive)
{
  SerdEnv* const  env         = serd_env_new(SERD_EMPTY_STRING());
  FilterPattern   pat         = {NULL, NULL, NULL, NULL};
  SerdSink*       in_sink     = serd_sink_new(&pat, on_filter_event, NULL);
  SerdByteSource* byte_source = serd_byte_source_new_string(str, NULL);
  SerdReader*     reader      = serd_reader_new(
    world, SERD_NQUADS, SERD_READ_VARIABLES, env, in_sink, 4096);

  SerdStatus st = serd_reader_start(reader, byte_source);
  if (!st) {
    st = serd_reader_read_document(reader);
  }

  serd_reader_free(reader);
  serd_env_free(env);
  serd_byte_source_free(byte_source);
  serd_sink_free(in_sink);

  if (st) {
    SERDI_ERRORF("error parsing filter pattern (%s)\n", serd_strerror(st));
    return NULL;
  }

  SerdSink* filter =
    serd_filter_new(sink, pat.s, pat.p, pat.o, pat.g, inclusive);

  serd_node_free(pat.s);
  serd_node_free(pat.p);
  serd_node_free(pat.o);
  serd_node_free(pat.g);
  return filter;
}

static SerdStatus
consume_source(SerdWorld* const      world,
               const Options         opts,
               SerdSyntax            syntax,
               SerdEnv* const        env,
               const SerdSink* const sink,
               SerdByteSource* const byte_source)
{
  if (!byte_source) {
    return SERD_ERR_UNKNOWN;
  }

  SerdStatus        st     = SERD_SUCCESS;
  SerdReader* const reader = serd_reader_new(
    world, syntax, opts.input_options.flags, env, sink, opts.stack_size);

  if (!(st = serd_reader_start(reader, byte_source))) {
    st = serd_reader_read_document(reader);
  }

  serd_reader_free(reader);
  serd_byte_source_free(byte_source);
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
    serd_open_input(filename, opts.block_size));
}

static SerdStatus
parse_option(OptionIter* iter, Options* const opts)
{
  const char  opt      = iter->argv[iter->a][iter->f];
  const char* argument = NULL;

  switch (opt) {
  case 'C':
    opts->canonical = true;
    return serd_option_iter_advance(iter);
  case 'f':
    opts->no_inline = true;
    opts->output_options.flags |= SERD_WRITE_VERBATIM;
    return serd_option_iter_advance(iter);
  case 'h':
    print_usage(iter->argv[0], false);
    return SERD_FAILURE;
  case 'm':
    opts->use_model = true;
    return serd_option_iter_advance(iter);
  case 'q':
    opts->quiet = true;
    return serd_option_iter_advance(iter);
  case 'v':
    return serd_print_version("serdi");
  case 'F':
    return serd_get_argument(iter, &opts->out_pattern);
  case 'G':
    return serd_get_argument(iter, &opts->in_pattern);
  case 'I':
    return serd_get_argument(iter, &opts->base_uri_string);
  case 'V':
  case 'X':
    // Just enable validation and skip the pattern, checks are parsed later
    opts->validate = opts->use_model = true;
    return serd_get_argument(iter, &argument);
  case 'c':
    return serd_get_argument(iter, &opts->chop_prefix);

  case 'b':
    return serd_get_size_argument(iter, &opts->block_size);

  case 'i':
    return serd_parse_input_argument(iter, &opts->input_options);

  case 'k':
    return serd_get_size_argument(iter, &opts->stack_size);

  case 'o':
    return serd_parse_output_argument(iter, &opts->output_options);

  case 'p':
    return serd_get_argument(iter, &opts->add_prefix);
  case 'r':
    return serd_get_argument(iter, &opts->root_uri);
  case 's':
    return serd_get_argument(iter, &opts->input_string);
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

static const SerdSink*
build_pipeline(SerdWorld*            world,
               const Options         opts,
               const SerdSink* const target,
               Pipeline* const       pipeline)
{
  const SerdSink* sink = target;

  if (opts.canonical) {
    sink = push_sink(pipeline,
                     serd_canon_new(world, sink, opts.input_options.flags));
  }

  if (opts.in_pattern) {
    sink =
      push_sink(pipeline, parse_filter(world, sink, opts.in_pattern, true));
  } else if (opts.out_pattern) {
    sink =
      push_sink(pipeline, parse_filter(world, sink, opts.out_pattern, false));
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

  const bool is_flat_output = opts.output_options.syntax == SERD_NQUADS ||
                              opts.output_options.syntax == SERD_NTRIPLES;

  if (!opts.base_uri_string && n_inputs == 1 && is_flat_output) {
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

static SerdWriter*
build_writer(SerdWorld* const     world,
             const Options        opts,
             const SerdEnv* const env,
             SerdByteSink* const  byte_sink)
{
  SerdWriter* const writer = serd_writer_new(world,
                                             opts.output_options.syntax,
                                             opts.output_options.flags,
                                             env,
                                             byte_sink);

  if (opts.root_uri) {
    serd_writer_set_root_uri(writer, SERD_STRING(opts.root_uri));
  }

  serd_writer_chop_blank_prefix(writer, opts.chop_prefix);
  return writer;
}

static SerdModel*
build_model(SerdWorld* const world, const Options opts, const bool with_graphs)
{
  SerdModel* const model =
    serd_model_new(world,
                   with_graphs ? SERD_ORDER_GSPO : SERD_ORDER_SPO,
                   with_graphs * SERD_STORE_GRAPHS);

  if (with_graphs) {
    serd_model_add_index(model, SERD_ORDER_GSPO);
  }

  if (opts.validate) {
    serd_model_add_index(model, SERD_ORDER_POS);
  }

  if (!opts.no_inline) {
    serd_model_add_index(model, SERD_ORDER_OPS);
    if (with_graphs) {
      serd_model_add_index(model, SERD_ORDER_GOPS);
    }
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
  if (opts.input_string) {
    st = consume_source(world,
                        opts,
                        opts.input_options.syntax ? opts.input_options.syntax
                                                  : SERD_TRIG,
                        env,
                        sink,
                        serd_byte_source_new_string(opts.input_string, NULL));
  }

  size_t prefix_len = 0;
  char*  prefix     = NULL;
  if (opts.n_inputs > 1) {
    prefix_len = 8 + strlen(opts.add_prefix);
    prefix     = (char*)calloc(1, prefix_len);
  }

  for (intptr_t i = 0; !st && i < opts.n_inputs; ++i) {
    if (opts.n_inputs > 1) {
      snprintf(prefix, prefix_len, "f%" PRIdPTR "%s", i, opts.add_prefix);
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

  if (!opts.output_options.syntax && !opts.output_options.overridden) {
    opts.output_options.syntax = with_graphs ? SERD_NQUADS : SERD_NTRIPLES;
  }

  const bool is_flat_output = opts.output_options.syntax == SERD_NQUADS ||
                              opts.output_options.syntax == SERD_NTRIPLES;

  SerdByteSink* const byte_sink =
    serd_open_output(opts.out_filename, opts.block_size);
  if (!byte_sink) {
    perror("serdi: error opening output file");
    return SERD_ERR_UNKNOWN;
  }

  SerdWorld* const  world    = serd_world_new();
  SerdEnv* const    env      = build_env(opts);
  SerdWriter* const writer   = build_writer(world, opts, env, byte_sink);
  SerdModel*        model    = NULL;
  Pipeline          pipeline = {0u, {NULL}};
  const SerdSink*   target   = NULL;
  if (opts.use_model) {
    model  = build_model(world, opts, with_graphs);
    target = push_sink(&pipeline, serd_inserter_new(model, NULL));
  } else {
    target = serd_writer_sink(writer);
  }

  const SerdSink* const sink = build_pipeline(world, opts, target, &pipeline);
  if (!sink) {
    SERDI_ERROR("failed to construct data pipeline, aborting\n");
    return SERD_ERR_BAD_ARG;
  }

  if (opts.quiet) {
    serd_world_set_log_func(world, serd_quiet_error_func, NULL);
  }

  SerdStatus st = read_inputs(world, opts, env, sink);

  if (st <= SERD_FAILURE && opts.use_model) {
    const SerdSink* writer_sink = serd_writer_sink(writer);
    SerdCursor*     everything  = serd_model_begin(model);

    serd_env_write_prefixes(env, writer_sink);

    st = serd_write_range(everything,
                          writer_sink,
                          ((opts.no_inline || is_flat_output) *
                           (SerdSerialisationFlags)SERD_NO_INLINE_OBJECTS));

    serd_cursor_free(everything);
  }

  if (st <= SERD_FAILURE && opts.validate) {
    SerdValidator* const validator = serd_validator_new(world);

    for (int i = 1; i < argc; ++i) {
      if (argv[i][0] == '-') {
        if (argv[i][1] == 'V') {
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
  serd_writer_free(writer);
  serd_env_free(env);
  serd_world_free(world);

  if (serd_byte_sink_close(byte_sink)) {
    perror("serdi: write error");
    st = SERD_ERR_UNKNOWN;
  }

  serd_byte_sink_free(byte_sink);

  return st;
}

int
main(int argc, char** argv)
{
  Options opts = {NULL,
                  NULL,
                  NULL,
                  NULL,
                  "",
                  "",
                  NULL,
                  NULL,
                  NULL,
                  0,
                  {SERD_SYNTAX_EMPTY, 0u, false},
                  {SERD_SYNTAX_EMPTY, 0u, false},
                  4096u,
                  4194304u,
                  false,
                  false,
                  false,
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

  if (opts.in_pattern && opts.out_pattern) {
    SERDI_ERROR("only one of -F and -G can be given at once\n");
    return EXIT_FAILURE;
  }

  // Every argument past the last option is an input
  opts.inputs   = argv + iter.a;
  opts.n_inputs = argc - iter.a;
  if (opts.n_inputs == 0 && !opts.input_string) {
    SERDI_ERROR("missing input\n");
    print_usage(argv[0], true);
    return EXIT_FAILURE;
  }

  if (opts.n_inputs + (bool)opts.input_string == 1) {
    opts.input_options.flags |= SERD_READ_GLOBAL;
  }

  st = st ? st : run(opts, argc, argv);

  return (st <= SERD_FAILURE) ? 0 : (int)st;
}
