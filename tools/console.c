// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include <serd/env.h>
#include <serd/file_uri.h>
#include <serd/input_stream.h>
#include <serd/status.h>
#include <serd/string.h>
#include <serd/version.h>
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

SerdInputStream
serd_open_tool_input(const char* const filename)
{
  return !strcmp(filename, "-") ? serd_open_input_standard()
                                : serd_open_input_file(filename);
}
