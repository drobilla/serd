// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/attributes.h"
#include "serd/status.h"

#include <assert.h>
#include <string.h>

static void
test_strerror(void)
{
  const char* msg = serd_strerror(SERD_SUCCESS);
  assert(!strcmp(msg, "Success"));

  for (int i = SERD_FAILURE; i <= SERD_BAD_DATA; ++i) {
    msg = serd_strerror((SerdStatus)i);
    assert(strcmp(msg, "Success"));
  }

  msg = serd_strerror((SerdStatus)-1);
  assert(!strcmp(msg, "Unknown error"));

  msg = serd_strerror((SerdStatus)1000000);
  assert(!strcmp(msg, "Unknown error"));
}

SERD_PURE_FUNC
int
main(void)
{
  test_strerror();

  return 0;
}
