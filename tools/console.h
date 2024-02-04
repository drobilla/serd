// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOOLS_CONSOLE_H
#define SERD_TOOLS_CONSOLE_H

#include "serd/env.h"
#include "serd/input_stream.h"
#include "serd/output_stream.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/attributes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// Iterator over command-line options with support for BSD-style flag merging
typedef struct {
  char* ZIX_NONNULL const* ZIX_NONNULL argv; ///< Argument vector (from main)
  int argc; ///< Number of arguments (from main)
  int a;    ///< Argument index (in argv)
  int f;    ///< Flag index (offset in argv[a])
} OptionIter;

/// Options for the input or output syntax
typedef struct {
  SerdSyntax syntax;     ///< User-specified syntax, or empty
  uint32_t   flags;      ///< SerdReaderFlags or SerdWriterFlags
  bool       overridden; ///< True if syntax was explicitly given
} SerdSyntaxOptions;

/// Options common to all command-line tools
typedef struct {
  const char* ZIX_NULLABLE base_uri;
  const char* ZIX_NULLABLE out_filename;
  size_t                   block_size;
  size_t                   stack_size;
  SerdSyntaxOptions        input;
  SerdSyntaxOptions        output;
} SerdCommonOptions;

/// Common "global" state of a command-line tool that writes data
typedef struct {
  SerdOutputStream            out;
  const char* ZIX_UNSPECIFIED name;
  SerdWorld* ZIX_UNSPECIFIED  world;
  SerdEnv* ZIX_UNSPECIFIED    env;
  SerdWriter* ZIX_UNSPECIFIED writer;
} SerdTool;

ZIX_NODISCARD bool
serd_option_iter_is_end(OptionIter iter);

ZIX_NODISCARD SerdStatus
serd_option_iter_advance(OptionIter* ZIX_NONNULL iter);

ZIX_CONST_FUNC ZIX_NODISCARD SerdCommonOptions
serd_default_options(void);

ZIX_NODISCARD SerdStatus
serd_tool_setup(SerdTool* ZIX_NONNULL   tool,
                const char* ZIX_NONNULL program,
                SerdCommonOptions       options);

SerdStatus
serd_tool_cleanup(SerdTool tool);

ZIX_NODISCARD SerdStatus
serd_print_version(const char* ZIX_NONNULL program);

ZIX_NODISCARD SerdSyntax
serd_choose_syntax(SerdTool* ZIX_NONNULL    tool,
                   SerdSyntaxOptions        options,
                   const char* ZIX_NULLABLE filename,
                   SerdSyntax               fallback);

ZIX_NODISCARD SerdStatus
serd_get_argument(OptionIter* ZIX_NONNULL              iter,
                  const char* ZIX_NONNULL* ZIX_NONNULL argument);

ZIX_NODISCARD SerdStatus
serd_get_size_argument(OptionIter* ZIX_NONNULL iter,
                       size_t* ZIX_NONNULL     argument);

ZIX_NODISCARD SerdStatus
serd_parse_common_option(OptionIter* ZIX_NONNULL        iter,
                         SerdCommonOptions* ZIX_NONNULL opts);

ZIX_NODISCARD SerdStatus
serd_read_source(SerdWorld* ZIX_NONNULL       world,
                 SerdCommonOptions            opts,
                 SerdEnv* ZIX_NONNULL         env,
                 SerdSyntax                   syntax,
                 SerdInputStream* ZIX_NONNULL in,
                 const char* ZIX_NONNULL      name,
                 const SerdSink* ZIX_NONNULL  sink);

ZIX_NODISCARD SerdStatus
serd_read_inputs(SerdTool* ZIX_NONNULL                tool,
                 SerdCommonOptions                    opts,
                 intptr_t                             n_inputs,
                 char* ZIX_NONNULL const* ZIX_NONNULL inputs,
                 const SerdSink* ZIX_NONNULL          sink);

#endif // SERD_TOOLS_CONSOLE_H
