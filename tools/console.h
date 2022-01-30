// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/serd.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Iterator over command-line options with support for BSD-style flag merging
typedef struct {
  char* const* argv; ///< Complete argument vector (from main)
  int          argc; ///< Total number of arguments (from main)
  int          a;    ///< Argument index (index into argv)
  int          f;    ///< Flag index (offset in argv[arg])
} OptionIter;

// Options for the input or output syntax
typedef struct {
  SerdSyntax syntax;     ///< User-specified syntax, or empty
  uint32_t   flags;      ///< SerdReaderFlags or SerdWriterFlags
  bool       overridden; ///< True if syntax was explicitly given
} SerdSyntaxOptions;

// Options common to all command-line tools
typedef struct {
  const char*       base_uri;
  const char*       out_filename;
  size_t            block_size;
  size_t            stack_size;
  SerdSyntaxOptions input;
  SerdSyntaxOptions output;
} SerdCommonOptions;

// Common "global" state of a command-line tool that writes data
typedef struct {
  SerdOutputStream out;
  SerdWorld*       world;
  SerdEnv*         env;
  SerdWriter*      writer;
} SerdTool;

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

SerdStatus
serd_tool_setup(SerdTool* tool, const char* program, SerdCommonOptions options);

SerdStatus
serd_tool_cleanup(SerdTool tool);

void
serd_set_stream_utf8_mode(FILE* stream);

SerdStatus
serd_print_version(const char* program);

SerdStatus
serd_get_argument(OptionIter* iter, const char** argument);

SerdStatus
serd_get_size_argument(OptionIter* iter, size_t* argument);

SerdStatus
serd_set_input_option(SerdStringView   name,
                      SerdSyntax*      syntax,
                      SerdReaderFlags* flags);

SerdStatus
serd_parse_input_argument(OptionIter* iter, SerdSyntaxOptions* options);

SerdStatus
serd_set_output_option(SerdStringView   name,
                       SerdSyntax*      syntax,
                       SerdWriterFlags* flags);

SerdStatus
serd_parse_output_argument(OptionIter* iter, SerdSyntaxOptions* options);

SerdStatus
serd_parse_common_option(OptionIter* iter, SerdCommonOptions* opts);

SerdEnv*
serd_create_env(SerdWorld*  world,
                const char* program,
                const char* base_string,
                const char* out_filename);

SerdSyntax
serd_choose_syntax(SerdWorld*        world,
                   SerdSyntaxOptions options,
                   const char*       filename,
                   SerdSyntax        fallback);

SerdInputStream
serd_open_tool_input(const char* filename);

SerdOutputStream
serd_open_tool_output(const char* filename);

SerdStatus
serd_read_source(SerdWorld*        world,
                 SerdCommonOptions opts,
                 SerdEnv*          env,
                 SerdSyntax        syntax,
                 SerdInputStream*  in,
                 const char*       name,
                 const SerdSink*   sink);

SerdStatus
serd_read_inputs(SerdWorld*        world,
                 SerdCommonOptions opts,
                 SerdEnv*          env,
                 intptr_t          n_inputs,
                 char* const*      inputs,
                 const SerdSink*   sink);
