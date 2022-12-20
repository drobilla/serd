// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/node.h"
#include "serd/status.h"
#include "serd/string.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
check_strlen(const char* const   str,
             const size_t        expected_n_bytes,
             const SerdNodeFlags expected_flags)
{
  SerdNodeFlags flags   = 0U;
  const size_t  n_bytes = serd_strlen(str, &flags);

  assert(n_bytes == expected_n_bytes);
  assert(flags == expected_flags);
}

static void
test_strlen(void)
{
  static const uint8_t utf8[] = {'"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0};

  check_strlen("\"quotes\"", 8U, SERD_HAS_QUOTE);
  check_strlen("newline\n", 8U, SERD_HAS_NEWLINE);
  check_strlen("\rreturn", 7U, SERD_HAS_NEWLINE);
  check_strlen((const char*)utf8, 7U, SERD_HAS_QUOTE | SERD_HAS_NEWLINE);

  assert(serd_strlen("nulls", NULL) == 5U);
}

static void
test_strerror(void)
{
  const char* msg = serd_strerror(SERD_SUCCESS);
  assert(!strcmp(msg, "Success"));
  for (int i = SERD_FAILURE; i <= SERD_BAD_LITERAL; ++i) {
    msg = serd_strerror((SerdStatus)i);
    assert(strcmp(msg, "Success"));
  }

  msg = serd_strerror((SerdStatus)-1);
  assert(!strcmp(msg, "Unknown error"));
}

ZIX_PURE_FUNC int
main(void)
{
  test_strlen();
  test_strerror();

  printf("Success\n");
  return 0;
}
