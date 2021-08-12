// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/byte_sink.h"
#include "serd/byte_source.h"
#include "serd/canon.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/filter.h"
#include "serd/log.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/filesystem.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERDI_ERROR(msg) fprintf(stderr, "serdi: " msg)
#define SERDI_ERRORF(fmt, ...) fprintf(stderr, "serdi: " fmt, __VA_ARGS__)

typedef struct {
  SerdNode* s;
  SerdNode* p;
  SerdNode* o;
  SerdNode* g;
} FilterPattern;

static int
print_usage(const char* const name, const bool error)
{
  static const char* const description =
    "Read and write RDF syntax.\n"
    "Use - for INPUT to read from standard input.\n\n"
    "  -C           Convert literals to canonical form.\n"
    "  -F PATTERN   Filter out statements that match PATTERN.\n"
    "  -G PATTERN   Only include statements matching PATTERN.\n"
    "  -I BASE_URI  Input base URI.\n"
    "  -b BYTES     I/O block size.\n"
    "  -f           Keep full URIs in input (don't qualify).\n"
    "  -h           Display this help and exit.\n"
    "  -i SYNTAX    Input syntax (turtle/ntriples/trig/nquads),\n"
    "               or flag (lax/variables/verbatim).\n"
    "  -k BYTES     Parser stack size.\n"
    "  -o SYNTAX    Output syntax (empty/turtle/ntriples/nquads),\n"
    "               or flag (ascii/expanded/verbatim/terse/lax).\n"
    "  -q           Suppress all output except data.\n"
    "  -r ROOT_URI  Keep relative URIs within ROOT_URI.\n"
    "  -s STRING    Parse STRING as input.\n"
    "  -v           Display version information and exit.\n"
    "  -w FILENAME  Write output to FILENAME instead of stdout.\n";

  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
  fprintf(os, "%s", description);
  return error ? 1 : 0;
}

static int
missing_arg(const char* const name, const char opt)
{
  SERDI_ERRORF("option requires an argument -- '%c'\n", opt);
  return print_usage(name, true);
}

static SerdStatus
on_filter_event(void* const handle, const SerdEvent* const event)
{
  if (event->type == SERD_STATEMENT) {
    FilterPattern* const pat = (FilterPattern*)handle;
    if (pat->s) {
      return SERD_ERR_BAD_DATA;
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
  SerdEnv* const  env         = serd_env_new(serd_empty_string());
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
read_file(SerdWorld* const      world,
          const SerdSyntax      syntax,
          const SerdReaderFlags flags,
          SerdEnv* const        env,
          const SerdSink* const sink,
          const size_t          stack_size,
          const char* const     filename,
          const size_t          block_size)
{
  SerdByteSource* byte_source = serd_open_input(filename, block_size);

  if (!byte_source) {
    SERDI_ERRORF(
      "failed to open input file `%s' (%s)\n", filename, strerror(errno));

    return SERD_ERR_UNKNOWN;
  }

  SerdReader* reader =
    serd_reader_new(world, syntax, flags, env, sink, stack_size);

  SerdStatus st = serd_reader_start(reader, byte_source);

  st = st ? st : serd_reader_read_document(reader);

  serd_reader_free(reader);
  serd_byte_source_free(byte_source);

  return st;
}

int
main(int argc, char** argv)
{
  const char* const prog = argv[0];

  SerdNode*       base          = NULL;
  SerdSyntax      input_syntax  = SERD_SYNTAX_EMPTY;
  SerdSyntax      output_syntax = SERD_SYNTAX_EMPTY;
  SerdReaderFlags reader_flags  = 0;
  SerdWriterFlags writer_flags  = 0;
  bool            osyntax_set   = false;
  bool            canonical     = false;
  bool            quiet         = false;
  size_t          block_size    = 4096U;
  size_t          stack_size    = 4194304U;
  const char*     input_string  = NULL;
  const char*     in_pattern    = NULL;
  const char*     out_pattern   = NULL;
  const char*     root_uri      = NULL;
  const char*     out_filename  = NULL;
  int             a             = 1;
  for (; a < argc && argv[a][0] == '-'; ++a) {
    if (argv[a][1] == '\0') {
      break;
    }

    for (int o = 1; argv[a][o]; ++o) {
      const char opt = argv[a][o];

      if (opt == 'C') {
        canonical = true;
      } else if (opt == 'f') {
        writer_flags |= (SERD_WRITE_EXPANDED | SERD_WRITE_VERBATIM);
      } else if (opt == 'h') {
        return print_usage(prog, false);
      } else if (opt == 'q') {
        quiet = true;
      } else if (opt == 'v') {
        return serd_print_version(argv[0]);
      } else if (argv[a][1] == 'F') {
        if (++a == argc) {
          return missing_arg(argv[0], 'F');
        }

        out_pattern = argv[a];
        break;
      } else if (argv[a][1] == 'G') {
        if (++a == argc) {
          return missing_arg(argv[0], 'g');
        }

        in_pattern = argv[a];
        break;
      } else if (argv[a][1] == 'I') {
        if (++a == argc) {
          return missing_arg(prog, 'I');
        }

        base = serd_new_uri(serd_string(argv[a]));
        break;
      } else if (opt == 'b') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'b');
        }

        char*      endptr = NULL;
        const long size   = strtol(argv[a], &endptr, 10);
        if (size < 1 || size == LONG_MAX || *endptr != '\0') {
          SERDI_ERRORF("invalid block size `%s'\n", argv[a]);
          return 1;
        }
        block_size = (size_t)size;
        break;
      } else if (opt == 'i') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'i');
        }

        if (serd_set_input_option(
              serd_string(argv[a]), &input_syntax, &reader_flags)) {
          return print_usage(argv[0], true);
        }
        break;
      } else if (opt == 'k') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'k');
        }

        char*      endptr = NULL;
        const long size   = strtol(argv[a], &endptr, 10);
        if (size <= 0 || size == LONG_MAX || *endptr != '\0') {
          SERDI_ERRORF("invalid stack size '%s'\n", argv[a]);
          return 1;
        }
        stack_size = (size_t)size;
        break;
      } else if (opt == 'o') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(prog, 'o');
        }

        if (serd_set_output_option(
              serd_string(argv[a]), &output_syntax, &writer_flags)) {
          return print_usage(argv[0], true);
        }

        osyntax_set =
          output_syntax != SERD_SYNTAX_EMPTY || !strcmp(argv[a], "empty");

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
      } else if (opt == 'w') {
        if (argv[a][o + 1] || ++a == argc) {
          return missing_arg(argv[0], 'w');
        }

        out_filename = argv[a];
        break;
      } else {
        SERDI_ERRORF("invalid option -- '%s'\n", argv[a] + 1);
        return print_usage(prog, true);
      }
    }
  }

  if (in_pattern && out_pattern) {
    SERDI_ERROR("only one of -F and -G can be given at once\n");
    return 1;
  }

  if (a == argc && !input_string) {
    SERDI_ERROR("missing input\n");
    return print_usage(prog, true);
  }

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
    if (!input_path || !(base = serd_new_file_uri(serd_string(input_path),
                                                  serd_empty_string()))) {
      SERDI_ERRORF("unable to determine base URI from path %s\n", inputs[0]);
    }
    zix_free(NULL, input_path);
  }

  SerdWorld* const world = serd_world_new();
  SerdEnv* const   env =
    serd_env_new(base ? serd_node_string_view(base) : serd_empty_string());

  serd_set_stream_utf8_mode(stdin);
  if (!out_filename) {
    serd_set_stream_utf8_mode(stdout);
  }

  SerdByteSink* const byte_sink = serd_open_output(out_filename, block_size);
  if (!byte_sink) {
    perror("serdi: error opening output file");
    return 1;
  }

  SerdWriter* const writer =
    serd_writer_new(world, output_syntax, writer_flags, env, byte_sink);

  const SerdSink* sink = serd_writer_sink(writer);

  SerdSink* canon = NULL;
  if (canonical) {
    sink = canon = serd_canon_new(world, sink, reader_flags);
  }

  SerdSink* filter = NULL;
  if (in_pattern) {
    if (!(filter = parse_filter(world, sink, in_pattern, true))) {
      SERDI_ERROR("error parsing inclusive filter pattern\n");
      return EXIT_FAILURE;
    }

    sink = filter;
  } else if (out_pattern) {
    if (!(filter = parse_filter(world, sink, out_pattern, false))) {
      SERDI_ERROR("error parsing exclusive filter pattern\n");
      return EXIT_FAILURE;
    }

    sink = filter;
  }

  if (quiet) {
    serd_set_log_func(world, serd_quiet_log_func, NULL);
  }

  if (root_uri) {
    serd_writer_set_root_uri(writer, serd_string(root_uri));
  }

  SerdStatus st = SERD_SUCCESS;
  if (input_string) {
    SerdByteSource* const byte_source =
      serd_byte_source_new_string(input_string, NULL);

    SerdReader* const reader =
      serd_reader_new(world,
                      input_syntax ? input_syntax : SERD_TRIG,
                      reader_flags,
                      env,
                      sink,
                      stack_size);

    if (!(st = serd_reader_start(reader, byte_source))) {
      st = serd_reader_read_document(reader);
    }

    serd_reader_free(reader);
    serd_byte_source_free(byte_source);
  }

  if (n_inputs == 1) {
    reader_flags |= SERD_READ_GLOBAL;
  }

  for (int i = 0; !st && i < n_inputs; ++i) {
    if (!base && !!strcmp(inputs[i], "-")) {
      if ((st = serd_set_base_uri_from_path(env, inputs[i]))) {
        SERDI_ERRORF("failed to set base URI from path %s\n", inputs[i]);
        break;
      }
    }

    if ((st =
           read_file(world,
                     serd_choose_input_syntax(world, input_syntax, inputs[i]),
                     reader_flags,
                     env,
                     sink,
                     stack_size,
                     inputs[i],
                     block_size))) {
      break;
    }
  }

  serd_sink_free(canon);
  serd_sink_free(filter);
  serd_writer_free(writer);
  serd_env_free(env);
  serd_node_free(base);
  serd_world_free(world);

  if (serd_byte_sink_close(byte_sink)) {
    perror("serdi: write error");
    st = SERD_ERR_UNKNOWN;
  }

  serd_byte_sink_free(byte_sink);

  return (st > SERD_FAILURE) ? 1 : 0;
}
