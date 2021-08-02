// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include <serd/input_stream.h>
#include <serd/version.h>

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

SerdInputStream
serd_open_tool_input(const char* const filename)
{
  return !strcmp(filename, "-") ? serd_open_input_standard()
                                : serd_open_input_file(filename);
}
