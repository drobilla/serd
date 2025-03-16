// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "system.h"

#include "serd_config.h"

#if USE_POSIX_FADVISE && USE_FILENO
#  include <fcntl.h>
#endif

#include <errno.h>
#include <string.h>

FILE*
serd_fopen(const char* const path, const char* const mode)
{
  FILE* fd = fopen(path, mode);
  if (!fd) {
    fprintf(
      stderr, "error: failed to open file %s (%s)\n", path, strerror(errno));
    return NULL;
  }

#if USE_POSIX_FADVISE && USE_FILENO
  (void)posix_fadvise(fileno(fd), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
  return fd;
}
