/*
  Copyright 2021 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void
test_default_log(void)
{
  SerdWorld* const world = serd_world_new();

  for (unsigned i = 0; i <= SERD_LOG_LEVEL_DEBUG; ++i) {
    const SerdLogLevel level = (SerdLogLevel)i;

    assert(!serd_world_logf(world, level, 0, NULL, "test"));
  }

  serd_world_free(world);
}

static SerdStatus
test_log(void* const handle, const SerdLogEntry* const entry)
{
  bool* const called = (bool*)handle;

  assert(entry->level == SERD_LOG_LEVEL_NOTICE);
  assert(entry->n_fields == 1);
  assert(!strcmp(entry->fields[0].key, "TEST_KEY"));
  assert(!strcmp(entry->fields[0].value, "TEST VALUE"));

  *called = true;
  return SERD_SUCCESS;
}

static void
test_custom_log(void)
{
  SerdWorld* const world  = serd_world_new();
  bool             called = false;

  serd_world_set_log_func(world, test_log, &called);

  const SerdLogField fields[1] = {{"TEST_KEY", "TEST VALUE"}};
  assert(!serd_world_logf(
    world, SERD_LOG_LEVEL_NOTICE, 1, fields, "test message %d", 42));

  serd_world_free(world);
}

int
main(void)
{
  test_default_log();
  test_custom_log();

  return 0;
}
