// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/cursor.h"
#include "serd/describe.h"
#include "serd/env.h"
#include "serd/inserter.h"
#include "serd/model.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/tee.h"
#include "serd/writer.h"
#include "zix/attributes.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// All options
typedef struct {
  SerdCommonOptions  common;
  const char*        collation;
  char* const*       inputs;
  intptr_t           n_inputs;
  SerdStatementOrder order;
  SerdDescribeFlags  flags;
} Options;

ZIX_PURE_FUNC static bool
input_has_graphs(const Options opts)
{
  if (opts.common.input.syntax) {
    return serd_syntax_has_graphs(opts.common.input.syntax);
  }

  for (intptr_t i = 0U; i < opts.n_inputs; ++i) {
    if (serd_syntax_has_graphs(serd_guess_syntax(opts.inputs[i]))) {
      return true;
    }
  }

  return false;
}

// Run the tool using the given options
static SerdStatus
run(const Options opts)
{
  SerdTool app = {{NULL, NULL, NULL}, NULL, NULL, NULL, NULL};

  // Set up the writing environment
  SerdStatus st = SERD_SUCCESS;
  if ((st = serd_tool_setup(&app, "serd-sort", opts.common))) {
    (void)serd_tool_cleanup(app);
    return st;
  }

  // Determine the default order to store statements in the model
  const bool               with_graphs   = input_has_graphs(opts);
  const SerdStatementOrder default_order = opts.collation ? opts.order
                                           : with_graphs  ? SERD_ORDER_GSPO
                                                          : SERD_ORDER_SPO;

  const SerdModelFlags flags =
    (SerdModelFlags)(with_graphs * SERD_STORE_GRAPHS);

  SerdModel* const model = serd_model_new(app.world, default_order, flags);

  if (!opts.collation) {
    // If we are pretty-printing, we need an O** index
    serd_model_add_index(model, SERD_ORDER_OPS);

    if (with_graphs) {
      // If we have graphs we still need the SPO index for finding subjects
      serd_model_add_index(model, SERD_ORDER_SPO);
    }
  }

  // Read all the inputs into an inserter to load the model
  SerdSink* const inserter = serd_inserter_new(model, NULL);
  SerdSink* const load_sink =
    serd_tee_new(NULL, serd_env_sink(app.env), inserter);
  if (st || (st = serd_read_inputs(
               &app, opts.common, opts.n_inputs, opts.inputs, load_sink))) {
    serd_sink_free(load_sink);
    serd_sink_free(inserter);
    serd_model_free(model);
    (void)serd_tool_cleanup(app);
    return st;
  }

  // Write the model to the output
  const SerdSink* const target = serd_writer_sink(app.writer);
  if (opts.collation) {
    SerdCursor* const cursor =
      serd_model_begin_ordered(NULL, model, opts.order);

    st = serd_env_describe(app.env, target);

    while (!st && !serd_cursor_is_end(cursor)) {
      st = serd_sink_write_statement(target, 0U, serd_cursor_get(cursor));
      serd_cursor_advance(cursor);
    }

    serd_cursor_free(NULL, cursor);
  } else {
    SerdCursor* const cursor = serd_model_begin(NULL, model);

    if (!(st = serd_env_describe(app.env, target))) {
      st = serd_describe_range(NULL, cursor, target, opts.flags);
    }

    serd_cursor_free(NULL, cursor);
  }

  if (!st) {
    st = serd_writer_finish(app.writer);
  }

  serd_sink_free(load_sink);
  serd_sink_free(inserter);
  serd_model_free(model);

  const SerdStatus cst = serd_tool_cleanup(app);
  return st ? st : cst;
}

/* Command-line interface (before setting up serd) */

static SerdStatus
parse_statement_order(const char* const string, SerdStatementOrder* const order)
{
  static const char* const strings[] = {"SPO",
                                        "SOP",
                                        "OPS",
                                        "OSP",
                                        "PSO",
                                        "POS",
                                        "GSPO",
                                        "GSOP",
                                        "GOPS",
                                        "GOSP",
                                        "GPSO",
                                        "GPOS",
                                        NULL};

  for (unsigned i = 0; strings[i]; ++i) {
    if (!strcmp(string, strings[i])) {
      *order = (SerdStatementOrder)i;
      return SERD_SUCCESS;
    }
  }

  return SERD_BAD_ARG;
}

static int
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Reorder RDF data by loading everything into a model then writing it.\n"
    "INPUT can be a local filename, or \"-\" to read from standard input.\n\n"
    "  -B BASE_URI   Base URI or path for resolving relative references.\n"
    "  -I SYNTAX     Input syntax turtle/ntriples/trig/nquads, or option\n"
    "                lax/variables/relative/global/generated.\n"
    "  -O SYNTAX     Output syntax empty/turtle/ntriples/nquads, or option\n"
    "                ascii/contextual/expanded/verbatim/terse/lax.\n"
    "  -V            Display version information and exit.\n"
    "  -b BYTES      I/O block size.\n"
    "  -c COLLATION  An optional \"G\" then the letters \"SPO\" in any order.\n"
    "  -h            Display this help and exit.\n"
    "  -k BYTES      Parser stack size.\n"
    "  -o FILENAME   Write output to FILENAME instead of stdout.\n"
    "  -t            Do not write type as \"a\" before other properties.\n";

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

  const char opt = iter->argv[iter->a][iter->f];
  switch (opt) {
  case 'V':
    return serd_print_version("serd-sort");

  case 'c':
    if (!(st = serd_get_argument(iter, &opts->collation))) {
      if ((st = parse_statement_order(opts->collation, &opts->order))) {
        ARG_ERRORF("unknown collation \"%s\"\n", opts->collation);
        return st;
      }
    }
    return st;

  case 'h':
    print_usage(iter->argv[0], false);
    return SERD_FAILURE;

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
  char  default_input[]  = "-";
  char* default_inputs[] = {default_input};

  Options opts = {serd_default_options(), NULL, NULL, 0U, SERD_ORDER_SPO, 0U};

  // Parse all command line options (which must precede inputs)
  SerdStatus st   = SERD_SUCCESS;
  OptionIter iter = {argv, argc, 1, 1};
  while (!serd_option_iter_is_end(iter)) {
    if ((st = parse_option(&iter, &opts))) {
      return (st == SERD_FAILURE) ? 0 : print_usage(argv[0], true);
    }
  }

  // Order statements to match longhand mode if necessary
  if (opts.common.output.flags & SERD_WRITE_LONGHAND) {
    opts.flags |= SERD_NO_TYPE_FIRST;
  }

  // Every argument past the last option is an input
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
