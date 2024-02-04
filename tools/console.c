// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include <serd/env.h>
#include <serd/input_stream.h>
#include <serd/output_stream.h>
#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <serd/string.h>
#include <serd/syntax.h>
#include <serd/uri.h>
#include <serd/version.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned WRITER_STACK_SIZE = 4096U;

#define LOG_ERR(prog, msg) fprintf(stderr, "%s: %s", prog, msg)
#define LOG_ERRF(prog, fmt, ...) fprintf(stderr, "%s: " fmt, prog, __VA_ARGS__)

ZIX_PURE_FUNC bool
serd_option_iter_is_end(const OptionIter iter)
{
  return iter.a >= iter.argc || iter.argv[iter.a][0] != '-' ||
         !iter.argv[iter.a][1];
}

SerdStatus
serd_option_iter_advance(OptionIter* const iter)
{
  if (!iter->argv[iter->a][++iter->f]) {
    ++iter->a;
    iter->f = 0;
  }

  return SERD_SUCCESS;
}

SerdCommonOptions
serd_default_options(void)
{
  const SerdCommonOptions opts = {
    NULL,
    NULL,
    4096U,
    524288U,
    {SERD_SYNTAX_EMPTY, 0U, false},
    {SERD_SYNTAX_EMPTY, 0U, false},
  };
  return opts;
}

static SerdOutputStream
serd_open_tool_output(const char* const filename)
{
  return (!filename || !strcmp(filename, "-"))
           ? serd_open_output_standard()
           : serd_open_output_file(filename);
}

static SerdStatus
serd_set_read_base(SerdEnv* const    env,
                   const char* const program,
                   const char* const base_string,
                   const char* const in_filename,
                   const char* const out_filename)
{
  if (serd_uri_string_has_scheme(base_string)) {
    return serd_env_set_base_uri(env, zix_string(base_string));
  }

  if (base_string && !strcmp(base_string, "rebase")) {
    if (!out_filename) {
      LOG_ERR(program, "rebase requires an output filename\n");
      return SERD_BAD_ARG;
    }

    return serd_env_set_base_path(env, zix_string(out_filename));
  }

  return serd_env_set_base_path(
    env, base_string ? zix_string(base_string) : zix_string(in_filename));
}

static SerdEnv*
serd_create_env(ZixAllocator* const allocator,
                const char* const   program,
                const char* const   base_string,
                const char* const   out_filename)
{
  SerdEnv* const env = serd_env_new(allocator, zix_empty_string());

  if (serd_set_read_base(env, program, base_string, NULL, out_filename)) {
    serd_env_free(env);
    return NULL;
  }

  return env;
}

SerdStatus
serd_tool_setup(SerdTool* const         tool,
                const char* const       program,
                const SerdCommonOptions opts)
{
  // Open the output first, since if that fails we have nothing to do
  const char* const out_path = opts.out_filename;
  errno                      = 0;
  if (!((tool->out = serd_open_tool_output(out_path)).stream)) {
    LOG_ERRF(program,
             "failed to open output \"%s\" (%s)\n",
             out_path,
             strerror(errno));
    return SERD_BAD_STREAM;
  }

  tool->name = program;

  const size_t out_block_size =
    tool->out.stream == stdout ? 1U : opts.block_size;

  // We have something to write to, so build the writing environment
  const SerdLimits limits = {opts.stack_size, WRITER_STACK_SIZE};
  if (!(tool->world = serd_world_new(NULL)) ||
      serd_world_set_limits(tool->world, limits) ||
      !(tool->env =
          serd_create_env(NULL, program, opts.base_uri, opts.out_filename)) ||
      !(tool->writer = serd_writer_new(
          tool->world,
          serd_choose_syntax(tool, opts.output, opts.out_filename, SERD_NQUADS),
          opts.output.flags,
          tool->env,
          &tool->out,
          out_block_size))) {
    LOG_ERR(program, "failed to set up writing environment\n");
    return SERD_UNKNOWN_ERROR;
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_tool_cleanup(SerdTool tool)
{
  SerdStatus st = SERD_SUCCESS;
  if (tool.out.stream) {
    // Close the output stream explicitly to check if there were any errors
    if ((st = serd_close_output(&tool.out))) {
      perror("write error");
    }
  }

  serd_writer_free(tool.writer);
  serd_env_free(tool.env);
  serd_world_free(tool.world);
  return st;
}

SerdStatus
serd_print_version(const char* const program)
{
  printf("%s (serd) %d.%d.%d\n",
         program,
         SERD_MAJOR_VERSION,
         SERD_MINOR_VERSION,
         SERD_MICRO_VERSION);
  return SERD_FAILURE;
}

SerdStatus
serd_get_argument(OptionIter* const iter, const char** const argument)
{
  const char flag = iter->argv[iter->a][iter->f];

  serd_option_iter_advance(iter);
  if (iter->a == iter->argc) {
    LOG_ERRF(iter->argv[0], "option requires an argument -- %c\n", flag);
    return SERD_BAD_ARG;
  }

  *argument = iter->argv[iter->a++] + iter->f;
  iter->f   = 0;
  return SERD_SUCCESS;
}

SerdStatus
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

SerdSyntax
serd_choose_syntax(const SerdTool* const   tool,
                   const SerdSyntaxOptions options,
                   const char* const       filename,
                   const SerdSyntax        fallback)
{
  if (options.overridden || options.syntax != SERD_SYNTAX_EMPTY) {
    return options.syntax;
  }

  if (!filename || !strcmp(filename, "-")) {
    return fallback;
  }

  const SerdSyntax guessed = serd_guess_syntax(filename);
  if (guessed != SERD_SYNTAX_EMPTY) {
    return guessed;
  }

  LOG_ERRF(tool->name,
           "unable to determine syntax of \"%s\", trying TriG\n",
           filename);

  return SERD_TRIG;
}

static SerdStatus
serd_set_input_option(const ZixStringView    name,
                      SerdSyntax* const      syntax,
                      SerdReaderFlags* const flags)
{
  typedef struct {
    const char*    name;
    SerdReaderFlag flag;
  } InputOption;

  static const InputOption input_options[] = {
    {"lax", SERD_READ_LAX},
    {"variables", SERD_READ_VARIABLES},
    {"generated", SERD_READ_GENERATED},
    {"global", SERD_READ_GLOBAL},
    {NULL, SERD_READ_LAX},
  };

  const SerdSyntax named_syntax = serd_syntax_by_name(name.data);
  if (!serd_strcasecmp(name.data, "empty") ||
      named_syntax != SERD_SYNTAX_EMPTY) {
    *syntax = named_syntax;
    return SERD_SUCCESS;
  }

  for (const InputOption* o = input_options; o->name; ++o) {
    if (!serd_strcasecmp(o->name, name.data)) {
      *flags |= o->flag;
      return SERD_SUCCESS;
    }
  }

  return SERD_BAD_ARG;
}

static SerdStatus
serd_parse_input_argument(OptionIter* const        iter,
                          SerdSyntaxOptions* const options)
{
  SerdStatus  st       = SERD_SUCCESS;
  const char* argument = NULL;

  if (!(st = serd_get_argument(iter, &argument))) {
    if ((st = serd_set_input_option(
           zix_string(argument), &options->syntax, &options->flags))) {
      LOG_ERRF(iter->argv[0], "unknown option \"%s\"\n", argument);
    } else if (!strcmp(argument, "empty") || options->syntax) {
      options->overridden = true;
    }
  }

  return st;
}

static SerdStatus
serd_set_output_option(const ZixStringView    name,
                       SerdSyntax* const      syntax,
                       SerdWriterFlags* const flags)
{
  typedef struct {
    const char*    name;
    SerdWriterFlag flag;
  } OutputOption;

  static const OutputOption output_options[] = {
    {"escaped", SERD_WRITE_ESCAPED},
    {"unqualified", SERD_WRITE_UNQUALIFIED},
    {"unresolved", SERD_WRITE_UNRESOLVED},
    {"lax", SERD_WRITE_LAX},
    {"terse", SERD_WRITE_TERSE},
    {NULL, SERD_WRITE_ESCAPED},
  };

  const SerdSyntax named_syntax = serd_syntax_by_name(name.data);
  if (!serd_strcasecmp(name.data, "empty") ||
      named_syntax != SERD_SYNTAX_EMPTY) {
    *syntax = named_syntax;
    return SERD_SUCCESS;
  }

  for (const OutputOption* o = output_options; o->name; ++o) {
    if (!serd_strcasecmp(o->name, name.data)) {
      *flags |= o->flag;
      return SERD_SUCCESS;
    }
  }

  return SERD_BAD_ARG;
}

static SerdStatus
serd_parse_output_argument(OptionIter* const        iter,
                           SerdSyntaxOptions* const options)
{
  SerdStatus  st       = SERD_SUCCESS;
  const char* argument = NULL;

  if (!(st = serd_get_argument(iter, &argument))) {
    if ((st = serd_set_output_option(
           zix_string(argument), &options->syntax, &options->flags))) {
      LOG_ERRF(iter->argv[0], "unknown option \"%s\"\n", argument);
    } else if (!strcmp(argument, "empty") || options->syntax) {
      options->overridden = true;
    }
  }

  return st;
}

SerdStatus
serd_parse_common_option(OptionIter* const iter, SerdCommonOptions* const opts)
{
  if (!iter->f && iter->argv[iter->a][0] == '-' && iter->argv[iter->a][1]) {
    ++iter->f;
  }

  const char opt = iter->argv[iter->a][iter->f];
  switch (opt) {
  case 'B':
    return serd_get_argument(iter, &opts->base_uri);

  case 'I':
    return serd_parse_input_argument(iter, &opts->input);

  case 'O':
    return serd_parse_output_argument(iter, &opts->output);

  case 'b':
    return serd_get_size_argument(iter, &opts->block_size);

  case 'k':
    return serd_get_size_argument(iter, &opts->stack_size);

  case 'o':
    return serd_get_argument(iter, &opts->out_filename);

  default:
    break;
  }

  return SERD_FAILURE;
}

SerdStatus
serd_read_source(SerdWorld* const        world,
                 const SerdCommonOptions opts,
                 const SerdSyntax        syntax,
                 SerdInputStream* const  in,
                 const char* const       name,
                 const SerdSink* const   sink)
{
  SerdReader* const reader =
    serd_reader_new(world, syntax, opts.input.flags, sink);

  const size_t block_size = in->stream == stdin ? 1U : opts.block_size;

  SerdStatus st = serd_reader_start(reader, in, zix_string(name), block_size);
  if (!st) {
    st = serd_reader_read_document(reader);
  }

  serd_reader_free(reader);
  return st;
}

static SerdInputStream
serd_open_tool_input(const char* const filename)
{
  return !strcmp(filename, "-") ? serd_open_input_standard()
                                : serd_open_input_file(filename);
}

SerdStatus
serd_read_inputs(SerdTool* const         tool,
                 const SerdCommonOptions opts,
                 const intptr_t          n_inputs,
                 char* const* const      inputs,
                 const SerdSink* const   sink)
{
  SerdWorld* const world = tool->world;
  SerdEnv* const   env   = tool->env;
  SerdStatus       st    = SERD_SUCCESS;

  for (intptr_t i = 0; !st && i < n_inputs; ++i) {
    // Use the filename as the base URI if possible if user didn't override it
    const char* const in_path = inputs[i];
    if (!opts.base_uri && !!strcmp(in_path, "-")) {
      serd_set_read_base(
        env, tool->name, opts.base_uri, in_path, opts.out_filename);
    }

    // Open the input stream
    errno              = 0;
    SerdInputStream in = serd_open_tool_input(in_path);
    if (!in.stream) {
      LOG_ERRF(tool->name,
               "failed to open input \"%s\" (%s)\n",
               in_path,
               strerror(errno));
      return SERD_BAD_ARG;
    }

    // Read the entire file
    st =
      serd_read_source(world,
                       opts,
                       serd_choose_syntax(tool, opts.input, in_path, SERD_TRIG),
                       &in,
                       !strcmp(in_path, "-") ? "stdin" : in_path,
                       sink);

    serd_close_input(&in);
  }

  return st;
}
