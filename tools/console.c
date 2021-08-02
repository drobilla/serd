// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include <serd/input_stream.h>
#include <serd/output_stream.h>
#include <serd/reader.h>
#include <serd/status.h>
#include <serd/string.h>
#include <serd/syntax.h>
#include <serd/version.h>
#include <serd/writer.h>
#include <zix/string_view.h>

#include <stdio.h>
#include <string.h>

int
serd_print_version(const char* const program)
{
  printf("%s (serd) %d.%d.%d\n",
         program,
         SERD_MAJOR_VERSION,
         SERD_MINOR_VERSION,
         SERD_MICRO_VERSION);
  return 0;
}

SerdSyntax
serd_choose_syntax(const SerdSyntax requested, const char* const filename)
{
  if (requested) {
    return requested;
  }

  const SerdSyntax guessed = serd_guess_syntax(filename);
  if (guessed != SERD_SYNTAX_EMPTY) {
    return guessed;
  }

  fprintf(stderr,
          "warning: unable to determine syntax of \"%s\", trying TriG\n",
          filename);

  return SERD_TRIG;
}

SerdStatus
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

  return SERD_FAILURE;
}

SerdStatus
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

  return SERD_FAILURE;
}

SerdInputStream
serd_open_tool_input(const char* const filename)
{
  return !strcmp(filename, "-") ? serd_open_input_standard()
                                : serd_open_input_file(filename);
}

SerdOutputStream
serd_open_tool_output(const char* const filename)
{
  return (!filename || !strcmp(filename, "-"))
           ? serd_open_output_standard()
           : serd_open_output_file(filename);
}
