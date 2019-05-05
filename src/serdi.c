/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#include "serd_config.h"
#include "system.h"

#include "serd/serd.h"

#ifdef _WIN32
#  ifdef _MSC_VER
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include <fcntl.h>
#  include <io.h>
#endif

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERDI_ERROR(msg) fprintf(stderr, "serdi: " msg)
#define SERDI_ERRORF(fmt, ...) fprintf(stderr, "serdi: " fmt, __VA_ARGS__)

static int
print_version(void)
{
  printf("serdi " SERD_VERSION " <http://drobilla.net/software/serd>\n");
  printf("Copyright 2011-2021 David Robillard <d@drobilla.net>.\n"
         "License: <http://www.opensource.org/licenses/isc>\n"
         "This is free software; you are free to change and redistribute it."
         "\nThere is NO WARRANTY, to the extent permitted by law.\n");
  return 0;
}

static int
print_usage(const char* const name, const bool error)
{
  FILE* const os = error ? stderr : stdout;
  fprintf(os, "%s", error ? "\n" : "");
  fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
  fprintf(os, "Read and write RDF syntax.\n");
  fprintf(os, "Use - for INPUT to read from standard input.\n\n");
  fprintf(os, "  -I BASE_URI  Input base URI.\n");
  fprintf(os, "  -a           Write ASCII output if possible.\n");
  fprintf(os, "  -b           Fast bulk output for large serialisations.\n");
  fprintf(os, "  -c PREFIX    Chop PREFIX from matching blank node IDs.\n");
  fprintf(os, "  -e           Eat input one character at a time.\n");
  fprintf(os, "  -f           Keep full URIs in input (don't qualify).\n");
  fprintf(os, "  -h           Display this help and exit.\n");
  fprintf(os, "  -i SYNTAX    Input syntax: turtle/ntriples/trig/nquads.\n");
  fprintf(os, "  -k BYTES     Parser stack size.\n");
  fprintf(os, "  -l           Lax (non-strict) parsing.\n");
  fprintf(os, "  -o SYNTAX    Output syntax: empty/turtle/ntriples/nquads.\n");
  fprintf(os, "  -p PREFIX    Add PREFIX to blank node IDs.\n");
  fprintf(os, "  -q           Suppress all output except data.\n");
  fprintf(os, "  -r ROOT_URI  Keep relative URIs within ROOT_URI.\n");
  fprintf(os, "  -s STRING    Parse STRING as input.\n");
  fprintf(os, "  -t           Write terser output without newlines.\n");
  fprintf(os, "  -v           Display version information and exit.\n");
  fprintf(os, "  -w FILENAME  Write output to FILENAME instead of stdout.\n");
  return error ? 1 : 0;
}

static int
missing_arg(const char* const name, const char opt)
{
  SERDI_ERRORF("option requires an argument -- '%c'\n", opt);
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
          SerdSyntax            syntax,
          const SerdReaderFlags flags,
          const SerdSink* const sink,
          const size_t          stack_size,
          const char* const     filename,
          const char* const     add_prefix,
          const bool            bulk_read)
{
  syntax = syntax ? syntax : serd_guess_syntax(filename);
  syntax = syntax ? syntax : SERD_TRIG;

  SerdStatus  st     = SERD_SUCCESS;
  SerdReader* reader = serd_reader_new(world, syntax, flags, sink, stack_size);

  serd_reader_add_blank_prefix(reader, add_prefix);

  if (!strcmp(filename, "-")) {
    SerdNode* name = serd_new_string(SERD_STATIC_STRING("stdin"));

    st = serd_reader_start_stream(
      reader, serd_file_read_byte, (SerdStreamErrorFunc)ferror, stdin, name, 1);

    serd_node_free(name);
  } else {
    st = serd_reader_start_file(reader, filename, bulk_read);
  }

  st = st ? st : serd_reader_read_document(reader);

  serd_reader_free(reader);

  return st;
}

int
main(int argc, char** argv)
{
  const char* const prog = argv[0];
  if (argc < 2) {
    return print_usage(prog, true);
  }

  SerdNode*       base          = NULL;
  SerdSyntax      input_syntax  = SERD_SYNTAX_EMPTY;
  SerdSyntax      output_syntax = SERD_SYNTAX_EMPTY;
  SerdReaderFlags reader_flags  = 0;
  SerdWriterFlags writer_flags  = 0;
  bool            bulk_read     = true;
  bool            bulk_write    = false;
  bool            osyntax_set   = false;
  bool            quiet         = false;
  size_t          stack_size    = 4194304;
  const char*     input_string  = NULL;
  const char*     add_prefix    = "";
  const char*     chop_prefix   = NULL;
  const char*     root_uri      = NULL;
  const char*     out_filename  = NULL;
  int             a             = 1;
  for (; a < argc && argv[a][0] == '-'; ++a) {
    if (argv[a][1] == '\0') {
      break;
    }

    for (int o = 1; argv[a][o]; ++o) {
      const char opt = argv[a][o];

      if (opt == 'a') {
        writer_flags |= SERD_WRITE_ASCII;
      } else if (opt == 'b') {
        bulk_write = true;
      } else if (opt == 'e') {
        bulk_read = false;
      } else if (opt == 'f') {
        writer_flags |= (SERD_WRITE_UNQUALIFIED | SERD_WRITE_UNRESOLVED);
      } else if (opt == 'h') {
        return print_usage(prog, false);
      } else if (opt == 'l') {
        reader_flags |= SERD_READ_LAX;
        writer_flags |= SERD_WRITE_LAX;
      } else if (opt == 'q') {
        quiet = true;
      } else if (opt == 't') {
        writer_flags |= SERD_WRITE_TERSE;
      } else if (opt == 'v') {
        return print_version();
      } else if (argv[a][1] == 'I') {
        if (++a == argc) {
          return missing_arg(prog, 'I');
        }

        base = serd_new_uri(SERD_MEASURE_STRING(argv[a]));
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
          SERDI_ERRORF("invalid stack size `%s'\n", argv[a]);
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

  if (a == argc && !input_string) {
    SERDI_ERROR("missing input\n");
    return 1;
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
    char* const input_path = serd_canonical_path(inputs[0]);
    if (!input_path ||
        !(base = serd_new_file_uri(SERD_MEASURE_STRING(input_path),
                                   SERD_EMPTY_STRING()))) {
      SERDI_ERRORF("unable to determine base URI from path %s\n", inputs[0]);
    }
    serd_free(input_path);
  }

  SerdWorld* const world = serd_world_new();
  SerdEnv* const   env =
    serd_env_new(base ? serd_node_string_view(base) : SERD_EMPTY_STRING());

#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  if (!out_filename) {
    _setmode(_fileno(stdout), _O_BINARY);
  }
#endif

  const size_t        block_size = bulk_write ? 4096u : 1u;
  SerdByteSink* const byte_sink =
    out_filename
      ? serd_byte_sink_new_filename(out_filename, block_size)
      : serd_byte_sink_new_function((SerdWriteFunc)fwrite, stdout, block_size);

  if (!byte_sink) {
    perror("serdi: error opening output file");
    return 1;
  }

  SerdWriter* const writer =
    serd_writer_new(world, output_syntax, writer_flags, env, byte_sink);

  if (quiet) {
    serd_world_set_error_func(world, quiet_error_func, NULL);
  }

  SerdNode* root = serd_new_uri(SERD_MEASURE_STRING(root_uri));
  serd_writer_set_root_uri(writer, root);
  serd_writer_chop_blank_prefix(writer, chop_prefix);
  serd_node_free(root);

  SerdStatus st         = SERD_SUCCESS;
  SerdNode*  input_name = NULL;
  if (input_string) {
    SerdReader* const reader =
      serd_reader_new(world,
                      input_syntax ? input_syntax : SERD_TRIG,
                      reader_flags,
                      serd_writer_sink(writer),
                      stack_size);

    serd_reader_add_blank_prefix(reader, add_prefix);

    SerdNode* name = serd_new_string(SERD_STATIC_STRING("string"));
    if (!(st = serd_reader_start_string(reader, input_string, name))) {
      st = serd_reader_read_document(reader);
    }

    serd_node_free(name);
    serd_reader_free(reader);
  }

  size_t prefix_len = 0;
  char*  prefix     = NULL;
  if (n_inputs > 1) {
    prefix_len = 8 + strlen(add_prefix);
    prefix     = (char*)calloc(1, prefix_len);
  }

  for (int i = 0; !st && i < n_inputs; ++i) {
    if (!base && strcmp(inputs[i], "-")) {
      char* const input_path = serd_canonical_path(inputs[i]);
      if (!input_path) {
        SERDI_ERRORF("failed to resolve path %s", inputs[i]);
        st = SERD_ERR_BAD_ARG;
        break;
      }

      SerdNode* const file_uri =
        serd_new_file_uri(SERD_MEASURE_STRING(input_path), SERD_EMPTY_STRING());

      serd_env_set_base_uri(env, serd_node_string_view(file_uri));
      serd_node_free(file_uri);
      serd_free(input_path);
    }

    if (n_inputs > 1) {
      snprintf(prefix, prefix_len, "f%d%s", i, add_prefix);
    }

    if ((st = read_file(world,
                        input_syntax,
                        reader_flags,
                        serd_writer_sink(writer),
                        stack_size,
                        inputs[i],
                        n_inputs > 1 ? prefix : add_prefix,
                        bulk_read))) {
      break;
    }
  }
  free(prefix);

  serd_writer_free(writer);
  serd_node_free(input_name);
  serd_env_free(env);
  serd_node_free(base);
  serd_world_free(world);

  if (serd_byte_sink_close(byte_sink) || (!out_filename && fclose(stdout))) {
    perror("serdi: write error");
    st = SERD_ERR_UNKNOWN;
  }

  serd_byte_sink_free(byte_sink);

  return (st > SERD_FAILURE) ? 1 : 0;
}
