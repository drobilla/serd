// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/serd.h"
#include "zix/allocator.h"
#include "zix/filesystem.h"
#include "zix/string_view.h"

#ifdef _WIN32
#  ifdef _MSC_VER
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include <fcntl.h>
#  include <io.h>
#endif

#include <stdint.h>
#include <string.h>

void
serd_set_stream_utf8_mode(FILE* const stream)
{
#ifdef _WIN32
  _setmode(_fileno(stream), _O_BINARY);
#else
  (void)stream;
#endif
}

int
serd_print_version(const char* const program)
{
  printf("%s %d.%d.%d <http://drobilla.net/software/serd>\n",
         program,
         SERD_MAJOR_VERSION,
         SERD_MINOR_VERSION,
         SERD_MICRO_VERSION);

  printf("Copyright 2011-2023 David Robillard <d@drobilla.net>.\n"
         "License: <http://www.opensource.org/licenses/isc>\n"
         "This is free software; you are free to change and redistribute it.\n"
         "There is NO WARRANTY, to the extent permitted by law.\n");

  return 0;
}

SerdStatus
serd_set_base_uri_from_path(SerdEnv* const env, const char* const path)
{
  char* const input_path = zix_canonical_path(NULL, path);
  if (!input_path) {
    return SERD_BAD_ARG;
  }

  SerdNode* const file_uri =
    serd_new_file_uri(NULL, zix_string(input_path), zix_empty_string());

  serd_env_set_base_uri(env, serd_node_string_view(file_uri));
  serd_node_free(NULL, file_uri);
  zix_free(NULL, input_path);

  return SERD_SUCCESS;
}

/// Wrapper for getc that is compatible with SerdReadFunc but faster than fread
static size_t
serd_file_read_byte(void* buf, size_t size, size_t nmemb, void* stream)
{
  (void)size;
  (void)nmemb;

  const int c = getc((FILE*)stream);
  if (c == EOF) {
    *((uint8_t*)buf) = 0;
    return 0;
  }

  *((uint8_t*)buf) = (uint8_t)c;
  return 1;
}

SerdInputStream
serd_open_tool_input(const char* const filename)
{
  if (!strcmp(filename, "-")) {
    const SerdInputStream in = serd_open_input_stream(
      serd_file_read_byte, (SerdErrorFunc)ferror, NULL, stdin);

    serd_set_stream_utf8_mode(stdin);
    return in;
  }

  return serd_open_input_file(filename);
}

SerdOutputStream
serd_open_tool_output(const char* const filename)
{
  if (!filename || !strcmp(filename, "-")) {
    serd_set_stream_utf8_mode(stdout);
    return serd_open_output_stream((SerdWriteFunc)fwrite,
                                   (SerdErrorFunc)ferror,
                                   (SerdCloseFunc)fclose,
                                   stdout);
  }

  return serd_open_output_file(filename);
}
