// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/env.h"
#include "serd/error.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/string_view.h"

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
    "  -a           Write ASCII output.\n"
    "  -b           Write output in blocks for performance.\n"
    "  -c PREFIX    Chop PREFIX from matching blank node IDs.\n"
    "  -e           Eat input one character at a time.\n"
    "  -f           Fast and loose URI pass-through.\n"
    "  -h           Display this help and exit.\n"
    "  -i SYNTAX    Input syntax: turtle/ntriples/trig/nquads.\n"
    "  -k BYTES     Parser stack size.\n"
    "  -l           Lax (non-strict) parsing.\n"
    "  -o SYNTAX    Output syntax: empty/turtle/ntriples/nquads.\n"
    "  -p PREFIX    Add PREFIX to blank node IDs.\n"
    "  -q           Suppress all output except data.\n"
    "  -r ROOT_URI  Keep relative URIs within ROOT_URI.\n"
    "  -s STRING    Parse STRING as input.\n"
    "  -t           Write terser output without newlines.\n"
    "  -v           Display version information and exit.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... INPUT\n", name);
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

int
main(int argc, char** argv)
{
  const char* const prog = argv[0];

  SerdNode*       base          = NULL;
  SerdSyntax      input_syntax  = SERD_SYNTAX_EMPTY;
  SerdSyntax      output_syntax = SERD_SYNTAX_EMPTY;
  SerdReaderFlags reader_flags  = 0U;
  SerdWriterFlags writer_flags  = 0U;
  bool            from_stdin    = false;
  bool            bulk_read     = true;
  bool            bulk_write    = false;
  bool            osyntax_set   = false;
  bool            quiet         = false;
  size_t          stack_size    = 524288U;
  const char*     input_string  = NULL;
  const char*     add_prefix    = NULL;
  const char*     chop_prefix   = NULL;
  const char*     root_uri      = NULL;
  int             a             = 1;
  for (; a < argc && argv[a][0] == '-'; ++a) {
    if (argv[a][1] == '\0') {
      from_stdin = true;
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

      if (opt == 'a') {
        writer_flags |= SERD_WRITE_ASCII;
      } else if (opt == 'b') {
        bulk_write = true;
      } else if (opt == 'e') {
        bulk_read = false;
      } else if (opt == 'f') {
        writer_flags |= (SERD_WRITE_UNQUALIFIED | SERD_WRITE_UNRESOLVED);
      } else if (opt == 'l') {
        reader_flags |= SERD_READ_LAX;
        writer_flags |= SERD_WRITE_LAX;
      } else if (opt == 'q') {
        quiet = true;
      } else if (opt == 't') {
        writer_flags |= SERD_WRITE_TERSE;
      } else if (argv[a][1] == 'B') {
        if (++a == argc) {
          return missing_arg(prog, 'B');
        }

        base = serd_new_uri(NULL, zix_string(argv[a]));
        break;
      } else if (opt == 'c') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'c');
        }

        chop_prefix = argv[a];
        break;
      } else if (opt == 'i') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'i');
        }

        if (!(input_syntax = serd_syntax_by_name(argv[a]))) {
          return print_usage(prog, true);
        }
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
        osyntax_set = true;
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'o');
        }

        if (!strcmp(argv[a], "empty")) {
          output_syntax = SERD_SYNTAX_EMPTY;
        } else if (!(output_syntax = serd_syntax_by_name(argv[a]))) {
          return print_usage(argv[0], true);
        }
        break;
      } else if (opt == 'p') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'p');
        }

        add_prefix = argv[a];
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
  serd_set_stream_utf8_mode(stdout);

  const char* input = argv[a++];

  if ((!input_syntax && !input) || !(input_syntax = serd_guess_syntax(input))) {
    input_syntax = SERD_TRIG;
  }

  const bool input_has_graphs = serd_syntax_has_graphs(input_syntax);
  if (!output_syntax && !osyntax_set) {
    output_syntax = input_has_graphs ? SERD_NQUADS : SERD_NTRIPLES;
  }

  if (!base && input) { // Use input file URI
    base = serd_new_file_uri(NULL, zix_string(input), zix_empty_string());
  }

  FILE* const      out_fd = stdout;
  SerdWorld* const world  = serd_world_new(NULL);
  SerdEnv* const   env =
    serd_env_new(NULL, base ? serd_node_string_view(base) : zix_empty_string());

  SerdOutputStream out = serd_open_output_stream((SerdWriteFunc)fwrite,
                                                 (SerdErrorFunc)ferror,
                                                 (SerdCloseFunc)fclose,
                                                 out_fd);

  SerdWriter* const writer = serd_writer_new(
    world, output_syntax, writer_flags, env, &out, bulk_write ? 4096U : 1U);

  const SerdLimits limits = {stack_size, MAX_DEPTH};
  serd_world_set_limits(world, limits);

  SerdReader* const reader = serd_reader_new(
    world, input_syntax, reader_flags, serd_writer_sink(writer));

  if (quiet) {
    serd_world_set_error_func(world, quiet_error_func, NULL);
  }

  if (root_uri) {
    serd_writer_set_root_uri(writer, zix_string(root_uri));
  }

  serd_writer_chop_blank_prefix(writer, chop_prefix);
  serd_reader_add_blank_prefix(reader, add_prefix);

  SerdStatus      st         = SERD_SUCCESS;
  SerdNode*       input_name = NULL;
  const char*     position   = NULL;
  SerdInputStream in         = {NULL, NULL, NULL, NULL};
  size_t          block_size = 1U;
  if (input_string) {
    position   = input_string;
    in         = serd_open_input_string(&position);
    input_name = serd_new_string(NULL, zix_string("string"));
  } else if (from_stdin) {
    in = serd_open_input_stream(
      (SerdReadFunc)fread, (SerdErrorFunc)ferror, (SerdCloseFunc)fclose, stdin);
    input_name = serd_new_string(NULL, zix_string("stdin"));
  } else {
    block_size = bulk_read ? 4096U : 1U;
    in         = serd_open_input_file(input);
    input_name = serd_new_string(NULL, zix_string(input));
  }

  if (!(st = serd_reader_start(reader, &in, input_name, block_size))) {
    st = serd_reader_read_document(reader);
  }

  serd_reader_finish(reader);
  serd_reader_free(reader);
  serd_writer_finish(writer);
  serd_writer_free(writer);
  serd_node_free(NULL, input_name);
  serd_env_free(env);
  serd_node_free(NULL, base);
  serd_world_free(world);

  if (fclose(stdout)) {
    perror("serd-pipe: write error");
    st = SERD_BAD_STREAM;
  }

  return (st > SERD_FAILURE) ? 1 : 0;
}
