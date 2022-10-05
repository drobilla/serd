// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "system.h"

#include "serd_config.h"

#ifdef _WIN32
#  include <malloc.h>
#endif

#include <string.h>

int
serd_system_strerror(const int errnum, char* const buf, const size_t buflen)
{
#if USE_STRERROR_R
  return strerror_r(errnum, buf, buflen);

#else // Not thread-safe, but... oh well?
  const char* const message = strerror(errnum);

  strncpy(buf, message, buflen);
  return 0;
#endif
}
