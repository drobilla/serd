// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/string.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
test_strlen(void)
{
  const uint8_t str[] = {'"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0};

  SerdNodeFlags flags   = 0;
  size_t        n_bytes = serd_strlen((const char*)str, &flags);
  assert(n_bytes == 7 && flags == (SERD_HAS_QUOTE | SERD_HAS_NEWLINE));
  assert(serd_strlen((const char*)str, NULL) == 7);
}

static void
test_strerror(void)
{
  const char* msg = serd_strerror(SERD_SUCCESS);
  assert(!strcmp(msg, "Success"));
  for (int i = SERD_FAILURE; i <= SERD_ERR_INTERNAL; ++i) {
    msg = serd_strerror((SerdStatus)i);
    assert(strcmp(msg, "Success"));
  }

  msg = serd_strerror((SerdStatus)-1);
  assert(!strcmp(msg, "Unknown error"));
}

SERD_PURE_FUNC
int
main(void)
{
  test_strlen();
  test_strerror();

  printf("Success\n");
  return 0;
}
