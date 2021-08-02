// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"
#include "system.h"

#include "serd/serd.h"

#ifdef _WIN32
#  ifdef _MSC_VER
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include <fcntl.h>
#  include <io.h>
#endif

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

SerdByteSource*
serd_open_input(const char* const filename, const size_t page_size)
{
  SerdByteSource* byte_source = NULL;
  if (!strcmp(filename, "-")) {
    serd_set_stream_utf8_mode(stdin);

    SerdNode* name = serd_new_string(serd_string("stdin"));

    byte_source = serd_byte_source_new_function(serd_file_read_byte,
                                                (SerdStreamErrorFunc)ferror,
                                                NULL,
                                                stdin,
                                                name,
                                                page_size);

    serd_node_free(name);
  } else {
    byte_source = serd_byte_source_new_filename(filename, page_size);
  }

  return byte_source;
}

SerdByteSink*
serd_open_output(const char* const filename, const size_t page_size)
{
  if (!filename || !strcmp(filename, "-")) {
    serd_set_stream_utf8_mode(stdout);
    return serd_byte_sink_new_function(
      (SerdWriteFunc)fwrite, stdout, page_size);
  }

  return serd_byte_sink_new_filename(filename, page_size);
}
