// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
test_strlen(void)
{
  const uint8_t str[] = {'"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0};

  size_t        n_bytes = 0;
  SerdNodeFlags flags   = 0;
  size_t        len     = serd_strlen(str, &n_bytes, &flags);
  assert(len == 5 && n_bytes == 7 &&
         flags == (SERD_HAS_QUOTE | SERD_HAS_NEWLINE));
  len = serd_strlen(str, NULL, &flags);
  assert(len == 5);

  assert(serd_strlen(str, &n_bytes, NULL) == 5);
}

static void
test_strerror(void)
{
  const uint8_t* msg = serd_strerror(SERD_SUCCESS);
  assert(!strcmp((const char*)msg, "Success"));
  for (int i = SERD_FAILURE; i <= SERD_ERR_INTERNAL; ++i) {
    msg = serd_strerror((SerdStatus)i);
    assert(strcmp((const char*)msg, "Success"));
  }

  msg = serd_strerror((SerdStatus)-1);
  assert(!strcmp((const char*)msg, "Unknown error"));
}

int
main(void)
{
  test_strlen();
  test_strerror();

  printf("Success\n");
  return 0;
}
