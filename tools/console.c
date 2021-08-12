// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include <serd/env.h>
#include <serd/file_uri.h>
#include <serd/input_stream.h>
#include <serd/output_stream.h>
#include <serd/reader.h>
#include <serd/status.h>
#include <serd/string.h>
#include <serd/syntax.h>
#include <serd/version.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/filesystem.h>
#include <zix/path.h>
#include <zix/string_view.h>

#ifdef _WIN32
#  ifdef _MSC_VER
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include <fcntl.h>
#  include <io.h>
#endif

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

static SerdString
base_uri_from_path(const char* const path)
{
  static const ZixStringView host = ZIX_STATIC_STRING("");

  SerdString base = {0, NULL};

  if (zix_path_is_absolute(path)) {
    char* const normal = zix_path_lexically_normal(NULL, path);

    base = serd_file_uri_to_string(NULL, zix_string(normal), host);

    zix_free(NULL, normal);
  } else {
    char* const cwd      = zix_current_path(NULL);
    char* const absolute = zix_path_join(NULL, cwd, path);
    char* const normal   = zix_path_lexically_normal(NULL, absolute);

    base = serd_file_uri_to_string(NULL, zix_string(normal), host);

    zix_free(NULL, normal);
    zix_free(NULL, absolute);
    zix_free(NULL, cwd);
  }

  return base;
}

SerdStatus
serd_set_base_uri_from_path(SerdEnv* const env, const char* const path)
{
  SerdString file_uri = base_uri_from_path(path);
  serd_env_set_base_uri(env, serd_string_view(file_uri));
  zix_free(NULL, file_uri.data);
  return SERD_SUCCESS;
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
    {"ascii", SERD_WRITE_ASCII},
    {"unqualified", SERD_WRITE_UNQUALIFIED},
    {"unresolved", SERD_WRITE_UNRESOLVED},
    {"lax", SERD_WRITE_LAX},
    {"terse", SERD_WRITE_TERSE},
    {NULL, SERD_WRITE_ASCII},
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
