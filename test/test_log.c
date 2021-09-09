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
#include <string.h>

static SerdStatus
custom_log_func(void* const               handle,
                const SerdLogLevel        level,
                const size_t              n_fields,
                const SerdLogField* const fields,
                const SerdStringView      message)
{
  (void)message;

  bool* const called = (bool*)handle;

  assert(level == SERD_LOG_LEVEL_NOTICE);
  assert(n_fields == 1);
  assert(!strcmp(fields[0].key, "TEST_KEY"));
  assert(!strcmp(fields[0].value, "TEST VALUE"));
  assert(!strcmp(message.buf, "test message 42"));
  assert(message.len == strlen("test message 42"));

  *called = true;
  return SERD_SUCCESS;
}

static void
test_bad_arg(void)
{
  SerdWorld* const world  = serd_world_new();
  bool             called = false;

  serd_set_log_func(world, custom_log_func, &called);

  assert(serd_logf(world, SERD_LOG_LEVEL_ERROR, "%s", "") == SERD_BAD_ARG);

  serd_world_free(world);
}

static void
test_default_log(void)
{
  SerdWorld* const world = serd_world_new();

  for (unsigned i = 0; i <= SERD_LOG_LEVEL_DEBUG; ++i) {
    const SerdLogLevel level = (SerdLogLevel)i;

    assert(!serd_logf(world, level, "test"));
  }

  serd_world_free(world);
}

static void
test_custom_log(void)
{
  SerdWorld* const world  = serd_world_new();
  bool             called = false;

  serd_set_log_func(world, custom_log_func, &called);

  const SerdLogField fields[1] = {{"TEST_KEY", "TEST VALUE"}};
  assert(!serd_xlogf(
    world, SERD_LOG_LEVEL_NOTICE, 1, fields, "test message %d", 42));

  assert(called);
  serd_world_free(world);
}

static void
test_caret(void)
{
  SerdWorld* const world = serd_world_new();
  SerdNode* const  name  = serd_new_string(SERD_STRING("filename"));
  SerdCaret* const caret = serd_caret_new(name, 46, 2);

  serd_logf_at(world, SERD_LOG_LEVEL_NOTICE, caret, "is just ahead of me");
  serd_logf_at(world, SERD_LOG_LEVEL_NOTICE, NULL, "isn't");

  serd_caret_free(caret);
  serd_node_free(name);
  serd_world_free(world);
}

static void
test_filename_only(void)
{
  SerdWorld* const world = serd_world_new();

  const SerdLogField fields[2] = {{"TEST_KEY", "TEST VALUE"},
                                  {"SERD_FILE", "somename"}};

  assert(!serd_xlogf(world, SERD_LOG_LEVEL_INFO, 2, fields, "no numbers here"));

  serd_world_free(world);
}

int
main(void)
{
  test_bad_arg();
  test_default_log();
  test_custom_log();
  test_caret();
  test_filename_only();

  return 0;
}
