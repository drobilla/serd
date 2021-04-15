// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "exess/exess.h"
#include "serd/string.h"

#include <assert.h>
#include <stdlib.h>

void*
serd_base64_decode(const char* const str, const size_t len, size_t* const size)
{
  assert(str);
  assert(size);

  const size_t max_size = exess_base64_decoded_size(len);

  void* const               buf = malloc(max_size);
  const ExessVariableResult r   = exess_read_base64(max_size, buf, str);
  if (r.status) {
    *size = 0;
    free(buf);
    return NULL;
  }

  *size = r.write_count;

  return buf;
}
