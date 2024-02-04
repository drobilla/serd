// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/env.h"
#include "serd/error.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/tee.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/filesystem.h"
#include "zix/string_view.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_ERR(msg) fprintf(stderr, "serd-pipe: " msg)
#define LOG_ERRF(fmt, ...) fprintf(stderr, "serd-pipe: " fmt, __VA_ARGS__)

#define MAX_DEPTH 128U

static int
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Read and write RDF syntax.\n"
    "Use - for INPUT to read from standard input.\n\n"
    "  -B BASE_URI  Base URI.\n"
    "  -I SYNTAX    Input syntax nquads/ntriples/trig/turtle, or option\n"
    "               generated/global/lax/prefixed/relative/variables.\n"
    "  -O SYNTAX    Output syntax empty/nquads/ntriples/trig/turtle, or\n"
    "               option ascii/contextual/expanded/lax/terse/verbatim.\n"
    "  -b BYTES     I/O block size.\n"
    "  -h           Display this help and exit.\n"
    "  -k BYTES     Parser stack size.\n"
    "  -o FILENAME  Write output to FILENAME instead of stdout.\n"
    "  -q           Suppress all output except data.\n"
    "  -r ROOT_URI  Keep relative URIs within ROOT_URI.\n"
    "  -s STRING    Parse STRING as input.\n"
    "  -v           Display version information and exit.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
  fprintf(os, "%s", description);
  return error ? 1 : 0;
}

static int
missing_arg(const char* const name, const char opt)
{
  LOG_ERRF("option requires an argument -- '%c'\n", opt);
  return print_usage(name, true);
}

static SerdStatus
quiet_error_func(void* const handle, const SerdError* const e)
{
  (void)handle;
  (void)e;
  return SERD_SUCCESS;
}

static SerdStatus
read_file(SerdWorld* const      world,
          const SerdSyntax      syntax,
          const SerdReaderFlags flags,
          SerdEnv* const        env,
          const SerdSink* const sink,
          const size_t          stack_size,
          const char* const     filename,
          const size_t          block_size)
{
  SerdInputStream in = serd_open_tool_input(filename);
  if (!in.stream) {
    LOG_ERRF(
      "failed to open input file `%s' (%s)\n", filename, strerror(errno));

    return SERD_BAD_STREAM;
  }

  const SerdLimits limits = {stack_size, MAX_DEPTH};
  serd_world_set_limits(world, limits);

  SerdReader* reader = serd_reader_new(world, syntax, flags, env, sink);
  SerdStatus  st     = serd_reader_start(reader, &in, NULL, block_size);

  st = st ? st : serd_reader_read_document(reader);

  serd_reader_free(reader);
  serd_close_input(&in);

  return st;
}

int
main(int argc, char** argv)
{
  const char* const prog = argv[0];

  SerdNode*       base          = NULL;
  SerdSyntax      input_syntax  = SERD_SYNTAX_EMPTY;
  SerdSyntax      output_syntax = SERD_SYNTAX_EMPTY;
  SerdReaderFlags reader_flags  = 0U;
  SerdWriterFlags writer_flags  = 0U;
  bool            osyntax_set   = false;
  bool            quiet         = false;
  size_t          block_size    = 4096U;
  size_t          stack_size    = 524288U;
  const char*     input_string  = NULL;
  const char*     out_filename  = NULL;
  const char*     root_uri      = NULL;
  int             a             = 1;
  for (; a < argc && argv[a][0] == '-'; ++a) {
    if (argv[a][1] == '\0') {
      break;
    }

    if (!strcmp(argv[a], "--help")) {
      return print_usage(prog, false);
    }

    if (!strcmp(argv[a], "--version")) {
      return serd_print_version(argv[0]);
    }

    for (int o = 1; argv[a][o]; ++o) {
      const char opt = argv[a][o];

      if (opt == 'h') {
        return print_usage(prog, false);
      }

      if (opt == 'v') {
        return serd_print_version(argv[0]);
      }

      if (opt == 'q') {
        quiet = true;
      } else if (opt == 'B') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'B');
        }

        base = serd_node_new(NULL, serd_a_uri_string(argv[a]));
        break;
      } else if (opt == 'I') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'I');
        }

        if (serd_set_input_option(
              zix_string(argv[a]), &input_syntax, &reader_flags)) {
          return print_usage(argv[0], true);
        }
        break;
      } else if (opt == 'O') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'O');
        }

        if (serd_set_output_option(
              zix_string(argv[a]), &output_syntax, &writer_flags)) {
          return print_usage(argv[0], true);
        }

        osyntax_set =
          output_syntax != SERD_SYNTAX_EMPTY || !strcmp(argv[a], "empty");

        break;
      } else if (opt == 'b') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'b');
        }

        char*      endptr = NULL;
        const long size   = strtol(argv[a], &endptr, 10);
        if (size < 1 || size == LONG_MAX || *endptr != '\0') {
          LOG_ERRF("invalid block size `%s'\n", argv[a]);
          return 1;
        }
        block_size = (size_t)size;
        break;
      } else if (opt == 'k') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'k');
        }

        char*      endptr = NULL;
        const long size   = strtol(argv[a], &endptr, 10);
        if (size <= 0 || size == LONG_MAX || *endptr != '\0') {
          LOG_ERRF("invalid stack size '%s'\n", argv[a]);
          return 1;
        }
        stack_size = (size_t)size;
        break;
      } else if (opt == 'o') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(argv[0], 'o');
        }

        out_filename = argv[a];
        break;
      } else if (opt == 'r') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'r');
        }

        root_uri = argv[a];
        break;
      } else if (opt == 's') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 's');
        }

        input_string = argv[a];
        break;
      } else {
        LOG_ERRF("invalid option -- '%s'\n", argv[a] + 1);
        return print_usage(prog, true);
      }
    }
  }

  if (a == argc && !input_string) {
    LOG_ERR("missing input\n");
    return print_usage(prog, true);
  }

  serd_set_stream_utf8_mode(stdin);

  char* const* const inputs   = argv + a;
  const int          n_inputs = argc - a;

  bool input_has_graphs = serd_syntax_has_graphs(input_syntax);
  for (int i = a; i < argc; ++i) {
    if (serd_syntax_has_graphs(serd_guess_syntax(argv[i]))) {
      input_has_graphs = true;
      break;
    }
  }

  if (!output_syntax && !osyntax_set) {
    output_syntax = input_has_graphs ? SERD_NQUADS : SERD_NTRIPLES;
  }

  if (!base && n_inputs == 1 &&
      (output_syntax == SERD_NQUADS || output_syntax == SERD_NTRIPLES)) {
    // Choose base URI from the single input path
    char* const input_path = zix_canonical_path(NULL, inputs[0]);
    if (!input_path ||
        !(base = serd_node_new(
            NULL,
            serd_a_file_uri(zix_string(input_path), zix_empty_string())))) {
      LOG_ERRF("unable to determine base URI from path %s\n", inputs[0]);
    }
    zix_free(NULL, input_path);
  }

  SerdWorld* const world  = serd_world_new(NULL);
  const SerdLimits limits = {stack_size, MAX_DEPTH};
  serd_world_set_limits(world, limits);

  SerdEnv* const env =
    serd_env_new(NULL, base ? serd_node_string_view(base) : zix_empty_string());

  SerdOutputStream out = serd_open_tool_output(out_filename);
  if (!out.stream) {
    perror("serdi: error opening output file");
    return 1;
  }

  SerdWriter* const writer =
    serd_writer_new(world, output_syntax, writer_flags, env, &out, block_size);

  if (quiet) {
    serd_world_set_error_func(world, quiet_error_func, NULL);
  }

  if (root_uri) {
    serd_writer_set_root_uri(writer, zix_string(root_uri));
  }

  SerdSink* const sink =
    serd_tee_new(NULL, serd_env_sink(env), serd_writer_sink(writer));

  SerdStatus st = SERD_SUCCESS;
  if (input_string) {
    const char*     position  = input_string;
    SerdInputStream string_in = serd_open_input_string(&position);

    SerdReader* const reader = serd_reader_new(
      world, input_syntax ? input_syntax : SERD_TRIG, reader_flags, env, sink);

    if (!(st = serd_reader_start(reader, &string_in, NULL, 1U))) {
      st = serd_reader_read_document(reader);
    }

    serd_reader_free(reader);
    serd_close_input(&string_in);
  }

  if (n_inputs == 1) {
    reader_flags |= SERD_READ_GLOBAL;
  }

  for (int i = 0; !st && i < n_inputs; ++i) {
    if (!base && !!strcmp(inputs[i], "-")) {
      if ((st = serd_set_base_uri_from_path(env, inputs[i]))) {
        LOG_ERRF("failed to set base URI from path %s\n", inputs[i]);
        break;
      }
    }

    if ((st = read_file(world,
                        serd_choose_syntax(input_syntax, inputs[i]),
                        reader_flags,
                        env,
                        sink,
                        stack_size,
                        inputs[i],
                        block_size))) {
      break;
    }
  }

  serd_sink_free(sink);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_node_free(NULL, base);
  serd_world_free(world);

  if (serd_close_output(&out)) {
    perror("serd-pipe: write error");
    st = SERD_BAD_STREAM;
  }

  return (st > SERD_FAILURE) ? 1 : 0;
}
