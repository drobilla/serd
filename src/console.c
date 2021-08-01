// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "console.h"

#ifdef _WIN32
#  ifdef _MSC_VER
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include <fcntl.h>
#  include <io.h>
#endif

void
serd_set_stream_utf8_mode(FILE* const stream)
{
#ifdef _WIN32
  _setmode(_fileno(stream), _O_BINARY);
#else
  (void)stream;
#endif
}
