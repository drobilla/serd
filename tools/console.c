// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#include "serd/serd.h"

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

  printf("Copyright 2011-2022 David Robillard <d@drobilla.net>.\n"
         "License: <http://www.opensource.org/licenses/isc>\n"
         "This is free software; you are free to change and redistribute it.\n"
         "There is NO WARRANTY, to the extent permitted by law.\n");

  return 0;
}

SerdStatus
serd_set_input_option(const SerdStringView   name,
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
    {"verbatim", SERD_READ_VERBATIM},
    {NULL, SERD_READ_LAX},
  };

  const SerdSyntax named_syntax = serd_syntax_by_name(name.buf);
  if (!serd_strncasecmp(name.buf, "empty", name.len) ||
      named_syntax != SERD_SYNTAX_EMPTY) {
    *syntax = named_syntax;
    return SERD_SUCCESS;
  }

  for (const InputOption* o = input_options; o->name; ++o) {
    if (!serd_strncasecmp(o->name, name.buf, name.len)) {
      *flags |= o->flag;
      return SERD_SUCCESS;
    }
  }

  //  SERDI_ERRORF("invalid input option `%s'\n", name.buf);
  return SERD_FAILURE;
}

SerdStatus
serd_set_output_option(const SerdStringView   name,
                       SerdSyntax* const      syntax,
                       SerdWriterFlags* const flags)
{
  typedef struct {
    const char*    name;
    SerdWriterFlag flag;
  } OutputOption;

  static const OutputOption output_options[] = {
    {"ascii", SERD_WRITE_ASCII},
    {"expanded", SERD_WRITE_EXPANDED},
    {"verbatim", SERD_WRITE_VERBATIM},
    {"terse", SERD_WRITE_TERSE},
    {"lax", SERD_WRITE_LAX},
    {NULL, SERD_WRITE_ASCII},
  };

  const SerdSyntax named_syntax = serd_syntax_by_name(name.buf);
  if (!serd_strncasecmp(name.buf, "empty", name.len) ||
      named_syntax != SERD_SYNTAX_EMPTY) {
    *syntax = named_syntax;
    return SERD_SUCCESS;
  }

  for (const OutputOption* o = output_options; o->name; ++o) {
    if (!serd_strncasecmp(o->name, name.buf, name.len)) {
      *flags |= o->flag;
      return SERD_SUCCESS;
    }
  }

  return SERD_FAILURE;
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

SerdByteSource*
serd_open_input(const char* const filename, const size_t block_size)
{
  SerdByteSource* byte_source = NULL;
  if (!strcmp(filename, "-")) {
    serd_set_stream_utf8_mode(stdin);

    SerdNode* name = serd_new_string(serd_string("stdin"));

    byte_source = serd_byte_source_new_function(
      serd_file_read_byte, (SerdStreamErrorFunc)ferror, NULL, stdin, name, 1);

    serd_node_free(name);
  } else {
    byte_source = serd_byte_source_new_filename(filename, block_size);
  }

  return byte_source;
}

SerdByteSink*
serd_open_output(const char* const filename, const size_t block_size)
{
  if (!filename || !strcmp(filename, "-")) {
    serd_set_stream_utf8_mode(stdout);
    return serd_byte_sink_new_function((SerdWriteFunc)fwrite, stdout, 1);
  }

  return serd_byte_sink_new_filename(filename, block_size);
}

SerdStatus
serd_set_base_uri_from_path(SerdEnv* const env, const char* const path)
{
  char* const input_path = serd_canonical_path(path);
  if (!input_path) {
    return SERD_ERR_BAD_ARG;
  }

  SerdNode* const file_uri =
    serd_new_file_uri(serd_string(input_path), serd_empty_string());

  serd_env_set_base_uri(env, serd_node_string_view(file_uri));
  serd_node_free(file_uri);
  serd_free(input_path);

  return SERD_SUCCESS;
}
