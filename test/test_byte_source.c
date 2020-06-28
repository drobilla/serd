// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static void
test_bad_page_size(void)
{
  assert(!serd_byte_source_new_filename("file.ttl", 0));

  assert(!serd_byte_source_new_function(
    (SerdReadFunc)fread, (SerdStreamErrorFunc)ferror, NULL, NULL, NULL, 0));
}

int
main(void)
{
  test_bad_page_size();

  return 0;
}
