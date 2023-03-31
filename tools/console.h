// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOOLS_CONSOLE_H
#define SERD_TOOLS_CONSOLE_H

#include "serd/env.h"
#include "serd/input_stream.h"
#include "serd/log.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

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
  SerdLogLevel      log_level;
} SerdCommonOptions;

// Common "global" state of a command-line tool that writes data
typedef struct {
  SerdOutputStream out;
  const char*      name;
  SerdWorld*       world;
  SerdEnv*         env;
  SerdWriter*      writer;
} SerdTool;

bool
serd_option_iter_is_end(OptionIter iter);

SerdStatus
serd_option_iter_advance(OptionIter* iter);

SerdCommonOptions
serd_default_options(void);

SerdStatus
serd_tool_setup(SerdTool* tool, const char* program, SerdCommonOptions options);

SerdStatus
serd_tool_cleanup(SerdTool tool);

void
serd_set_stream_utf8_mode(FILE* stream);

SerdStatus
serd_print_version(const char* program);

SerdSyntax
serd_choose_syntax(SerdTool*         tool,
                   SerdSyntaxOptions options,
                   const char*       filename,
                   SerdSyntax        fallback);

SerdStatus
serd_get_argument(OptionIter* iter, const char** argument);

SerdStatus
serd_get_size_argument(OptionIter* iter, size_t* argument);

SerdStatus
serd_set_input_option(ZixStringView    name,
                      SerdSyntax*      syntax,
                      SerdReaderFlags* flags);

SerdStatus
serd_parse_input_argument(OptionIter* iter, SerdSyntaxOptions* options);

SerdStatus
serd_set_output_option(ZixStringView    name,
                       SerdSyntax*      syntax,
                       SerdWriterFlags* flags);

SerdStatus
serd_parse_output_argument(OptionIter* iter, SerdSyntaxOptions* options);

SerdStatus
serd_parse_common_option(OptionIter* iter, SerdCommonOptions* opts);

SerdEnv*
serd_create_env(ZixAllocator* allocator,
                const char*   program,
                const char*   base_string,
                const char*   out_filename);

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
serd_read_inputs(SerdTool*         tool,
                 SerdCommonOptions opts,
                 intptr_t          n_inputs,
                 char* const*      inputs,
                 const SerdSink*   sink);

#endif // SERD_TOOLS_CONSOLE_H
