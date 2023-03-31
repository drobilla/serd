// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/status.h"
#include "zix/attributes.h"

#include <assert.h>
#include <string.h>

static void
test_strerror(void)
{
  const char* msg = serd_strerror(SERD_SUCCESS);
  assert(!strcmp(msg, "Success"));
  for (int i = SERD_FAILURE; i <= SERD_BAD_INDEX; ++i) {
    msg = serd_strerror((SerdStatus)i);
    assert(strcmp(msg, "Success"));
  }

  msg = serd_strerror((SerdStatus)-1);
  assert(!strcmp(msg, "Unknown error"));
}

ZIX_PURE_FUNC int
main(void)
{
  test_strerror();

  return 0;
}
