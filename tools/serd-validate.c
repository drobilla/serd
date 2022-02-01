/*
  Copyright 2011-2022 David Robillard <d@drobilla.net>

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

/* #include "console.h" */

#include "serd/serd.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_OWL "http://www.w3.org/2002/07/owl#"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_RDFS "http://www.w3.org/2000/01/rdf-schema#"

/* Application (after parsing command-line arguments) */

typedef struct {
  const char*  base_uri_string;
  const char*  out_filename;
  char* const* inputs;
  intptr_t     n_inputs;
  /* SerdSyntaxOptions input_options; */
  size_t block_size;
  size_t stack_size;
  bool   checks_given;
  bool   verbose;
  bool   quiet;
} Options;

typedef struct {
  SerdWorld*     world;
  SerdEnv*       env;
  SerdModel*     model;
  SerdValidator* validator;
} Tool;

static SerdStatus
read_file(SerdWorld* const      world,
          const Options         opts,
          SerdEnv* const        env,
          const SerdSink* const sink,
          const char* const     filename)
{
  SerdStatus st = SERD_SUCCESS;
  if (!opts.base_uri_string && strcmp(filename, "-")) {
    if ((st = serd_env_set_base_path(env, SERD_STRING(filename)))) {
      serd_logf(world,
                SERD_LOG_LEVEL_ERROR,
                "failed to determine base URI from path %s",
                filename);
      return st;
    }
  }

  const SerdSyntax syntax = SERD_TURTLE;
  /* serd_choose_syntax(world, opts.input_options, filename, SERD_TRIG); */

  SerdReader* const reader = serd_reader_new(
    world, syntax, 0u /*opts.input_options.flags*/, env, sink, opts.stack_size);

  const SerdNode* const name =
    serd_nodes_string(serd_world_nodes(world), SERD_STRING(filename));

  SerdInputStream in = serd_open_input_file(filename);
  if (!in.stream) {
    st = SERD_UNKNOWN_ERROR;
  } else if (!(st = serd_reader_start(reader, &in, name, opts.block_size))) {
    st = serd_reader_read_document(reader);
  }

  serd_close_input(&in);
  serd_reader_free(reader);
  return st;
}

/*
  Return a model where every object is the file URI of a schema to load.

  The statements in the result are like `?ontology rdfs:seeAlso ?resource`,
  where `?ontology` is the URI of the owl:Ontology instance and `?resource` is
  a file URI.
*/
static SerdModel*
get_schemas_model(const Options    opts,
                  SerdWorld* const world,
                  SerdModel* const model)
{
  static const SerdStringView s_rdf_type     = SERD_STRING(NS_RDF "type");
  static const SerdStringView s_owl_Ontology = SERD_STRING(NS_OWL "Ontology");
  static const SerdStringView s_rdfs_seeAlso = SERD_STRING(NS_RDFS "seeAlso");

  SerdNodes* const nodes = serd_world_nodes(world);
  SerdModel* const schemas_model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_CARETS);

  const SerdNode* const rdf_type     = serd_nodes_uri(nodes, s_rdf_type);
  const SerdNode* const owl_Ontology = serd_nodes_uri(nodes, s_owl_Ontology);
  const SerdNode* const rdfs_seeAlso = serd_nodes_uri(nodes, s_rdfs_seeAlso);

  SerdCursor* const i =
    serd_model_find(model, NULL, rdf_type, owl_Ontology, NULL);

  for (; !serd_cursor_is_end(i); serd_cursor_advance(i)) {
    const SerdStatement* const typing   = serd_cursor_get(i);
    const SerdNode* const      ontology = serd_statement_subject(typing);

    const SerdStatement* const link =
      serd_model_get_statement(model, ontology, rdfs_seeAlso, NULL, NULL);
    if (link) {
      const SerdNode* const resource = serd_statement_object(link);
      if (resource && serd_node_type(resource) == SERD_URI) {
        if (opts.verbose) {
          serd_logf_at(world,
                       SERD_LOG_LEVEL_NOTICE,
                       serd_statement_caret(link),
                       "Loading %s",
                       serd_node_string(resource));
        }

        const char* const resource_uri = serd_node_string(resource);
        if (!strncmp(resource_uri, "file://", strlen("file://"))) {
          serd_model_insert(schemas_model, link);
        }
      }
    }
  }

  serd_cursor_free(i);

  return schemas_model;
}

static SerdStatus
run(Tool* tool, Options opts, int argc, char** argv)
{
  SerdWorld* const world = tool->world;
  SerdNodes* const nodes = serd_world_nodes(world);
  SerdEnv* const   env   = tool->env;
  SerdModel* const model = tool->model;

  const SerdNode* const schema_graph =
    serd_nodes_uri(nodes, SERD_STRING("http://drobilla.net/sw/serd#schemas"));

  const SerdNode* const data_graph =
    serd_nodes_uri(nodes, SERD_STRING("http://drobilla.net/sw/serd#data"));

  serd_model_add_index(model, SERD_ORDER_GSPO);
  serd_model_add_index(model, SERD_ORDER_POS);
  serd_model_add_index(model, SERD_ORDER_GPOS);
  serd_model_add_index(model, SERD_ORDER_PSO);
  serd_model_add_index(model, SERD_ORDER_GPSO);
  serd_model_add_index(model, SERD_ORDER_GOPS);

  if (opts.quiet) {
    serd_set_log_func(world, serd_quiet_log_func, NULL);
  }

  SerdStatus st = SERD_SUCCESS;

  // Read inputs into the data graph
  {
    SerdSink* const data_sink = serd_inserter_new(model, data_graph);

    for (intptr_t i = 0; !st && i < opts.n_inputs; ++i) {
      st = read_file(world, opts, env, data_sink, opts.inputs[i]);
    }

    serd_sink_free(data_sink);
  }

  if (st <= SERD_FAILURE) { // FIXME: ?
    SerdValidator* const validator = tool->validator;

    SerdSink* const schema_sink = serd_inserter_new(model, schema_graph);

    for (int i = 1; i < argc; ++i) {
      if (argv[i][0] == '-') {
        if (argv[i][1] == 's') {
          st = read_file(world, opts, env, schema_sink, argv[++i]);
        }
      }
    }

    {
      SerdModel* const schemas_model = get_schemas_model(opts, world, model);

      SerdCursor* const i = serd_model_begin(schemas_model);
      for (; !serd_cursor_is_end(i); serd_cursor_advance(i)) {
        const SerdStatement* const link         = serd_cursor_get(i);
        const SerdNode* const      resource     = serd_statement_object(link);
        const char* const          resource_uri = serd_node_string(resource);

        char* const path = serd_parse_file_uri(NULL, resource_uri, NULL);
        if (path) {
          st = read_file(world, opts, env, schema_sink, path);
          serd_free(NULL, path);
        }
      }

      serd_cursor_free(i);
      serd_model_free(schemas_model);
    }

    serd_sink_free(schema_sink);

    if (!st) {
      serd_validator_set_log_env(validator, env);
      st = serd_validate(validator, model, data_graph);
    }
  }

  return st;
}

/* Command-line interface (before setting up serd) */

// Iterator over command-line options with support for BSD-style flag merging
typedef struct {
  char* const* argv; ///< Complete argument vector (from main)
  int          argc; ///< Total number of arguments (from main)
  int          a;    ///< Argument index (index into argv)
  int          f;    ///< Flag index (offset in argv[arg])
} OptionIter;

static inline bool
serd_option_iter_is_end(const OptionIter iter)
{
  return iter.a >= iter.argc || iter.argv[iter.a][0] != '-' ||
         !iter.argv[iter.a][iter.f];
}

static inline SerdStatus
serd_option_iter_advance(OptionIter* const iter)
{
  if (!iter->argv[iter->a][++iter->f]) {
    ++iter->a;
    iter->f = 1;
  }

  return SERD_SUCCESS;
}

static SerdStatus
serd_get_argument(OptionIter* const iter, const char** const argument)
{
  const char flag = iter->argv[iter->a][iter->f++];

  if (iter->argv[iter->a][iter->f] || (iter->a + 1) == iter->argc) {
    fprintf(
      stderr, "%s: option requires an argument -- %c\n", iter->argv[0], flag);
    return SERD_BAD_ARG;
  }

  *argument = iter->argv[++iter->a];
  ++iter->a;
  iter->f = 1;
  return SERD_SUCCESS;
}

static SerdStatus
serd_get_size_argument(OptionIter* const iter, size_t* const argument)
{
  SerdStatus  st     = SERD_SUCCESS;
  const char* string = NULL;
  if ((st = serd_get_argument(iter, &string))) {
    return st;
  }

  char*      endptr = NULL;
  const long size   = strtol(string, &endptr, 10);
  if (size <= 0 || size == LONG_MAX || *endptr != '\0') {
    return SERD_BAD_ARG;
  }

  *argument = (size_t)size;
  return SERD_SUCCESS;
}

static SerdStatus
print_version(void)
{
  printf("serd-validate %d.%d.%d <http://drobilla.net/software/serd>\n",
         SERD_MAJOR_VERSION,
         SERD_MINOR_VERSION,
         SERD_MICRO_VERSION);

  printf("Copyright 2011-2022 David Robillard <d@drobilla.net>.\n"
         "License: <http://www.opensource.org/licenses/isc>\n"
         "This is free software; you are free to change and redistribute it.\n"
         "There is NO WARRANTY, to the extent permitted by law.\n");

  return SERD_FAILURE;
}

static SerdStatus
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Validate RDF data against RDFS and OWL schemas.\n"
    "INPUT can be a local filename, or \"-\" to read from standard input.\n\n"
    "  -B BASE_URI  Base URI or path for resolving relative references.\n"
    "  -I SYNTAX    Input syntax (turtle/ntriples/trig/nquads),\n"
    "               or option (lax/variables/relative/global/generated).\n"
    "  -V           Display version information and exit.\n"
    "  -W CHECKS    Enable checks matching regex CHECKS (or \"all\").\n"
    "  -X CHECKS    Exclude checks matching regex CHECKS (or \"all\").\n"
    "  -h           Display this help and exit.\n"
    "  -k BYTES     Parser stack size.\n"
    "  -v           Print verbose messages about loaded resources.\n"
    "  -s SCHEMA    Schema input file.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
  fprintf(os, "%s", description);
  return error ? SERD_BAD_ARG : SERD_SUCCESS;
}

static SerdStatus
parse_option(Tool* const tool, OptionIter* const iter, Options* const opts)
{
  SerdStatus  st       = SERD_SUCCESS;
  const char  opt      = iter->argv[iter->a][iter->f];
  const char* argument = NULL;

  switch (opt) {
  case 'B':
    if (!(st = serd_get_argument(iter, &argument))) {
      opts->base_uri_string = argument;
      st = serd_env_set_base_uri(tool->env, SERD_STRING(argument));
    }
    return st;

    /* case 'I': */
    /*   return serd_parse_input_argument(iter, &opts->input_options); */

  case 'V':
    return print_version();

  case 'W':
    opts->checks_given = true;
    if (!(st = serd_get_argument(iter, &argument))) {
      st = serd_validator_enable_checks(tool->validator, argument);
    }
    return st;

  case 'X':
    opts->checks_given = true;
    if (!(st = serd_get_argument(iter, &argument))) {
      st = serd_validator_disable_checks(tool->validator, argument);
    }
    return st;

  case 'h':
    print_usage(iter->argv[0], false);
    return SERD_FAILURE;

  case 'k':
    return serd_get_size_argument(iter, &opts->stack_size);

  case 'q':
    opts->quiet = true;
    return serd_option_iter_advance(iter);

  case 's':
    // Schema input, ignore here since these are loaded later
    return serd_get_argument(iter, &argument);

  case 'v':
    opts->verbose = true;
    return serd_option_iter_advance(iter);

  case 'w':
    return serd_get_argument(iter, &opts->out_filename);

  default:
    break;
  }

  fprintf(stderr, "%s: invalid option -- '%c'\n", iter->argv[0], opt);
  return print_usage(iter->argv[0], true);
}

static void
free_tool(Tool* const tool)
{
  serd_validator_free(tool->validator);
  serd_model_free(tool->model);
  serd_env_free(tool->env);
  serd_world_free(tool->world);
}

int
main(int argc, char** argv)
{
  Options opts = {NULL,
                  NULL,
                  NULL,
                  0,
                  //                  {SERD_SYNTAX_EMPTY, 0u, false},
                  4096u,
                  4194304u,
                  false,
                  false,
                  false};

  SerdWorld* const     world     = serd_world_new(NULL);
  SerdEnv* const       env       = serd_env_new(world, SERD_EMPTY_STRING());
  const SerdModelFlags flags     = SERD_STORE_GRAPHS | SERD_STORE_CARETS;
  SerdModel* const     model     = serd_model_new(world, SERD_ORDER_SPO, flags);
  SerdValidator* const validator = serd_validator_new(world);
  Tool                 tool      = {world, env, model, validator};

  // Parse all command line options (which must precede inputs)
  SerdStatus st   = SERD_SUCCESS;
  OptionIter iter = {argv, argc, 1, 1};
  while (!serd_option_iter_is_end(iter)) {
    if ((st = parse_option(&tool, &iter, &opts))) {
      free_tool(&tool);
      return (int)SERD_BAD_ARG;
    }
  }

  // If no checks were given, enable "all" by default
  if (!opts.checks_given) {
    serd_validator_enable_checks(validator, "all");
  }

  // Every argument past the last option is an input
  opts.inputs   = argv + iter.a;
  opts.n_inputs = argc - iter.a;
  if (opts.n_inputs == 0) {
    fprintf(stderr, "%s: missing input\n", argv[0]);
    free_tool(&tool);
    st = print_usage(argv[0], true);
    return (int)st;
  }

  if (!opts.base_uri_string && opts.n_inputs == 1) {
    st = serd_env_set_base_path(env, SERD_STRING(opts.inputs[0]));
  }

  if (!st) {
    st = run(&tool, opts, argc, argv);
  }

  free_tool(&tool);

  return (st <= SERD_FAILURE) ? 0 : (int)st;
}
