// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/log.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static SerdStatus
custom_log_func(void* const               handle,
                const SerdLogLevel        level,
                const size_t              n_fields,
                const SerdLogField* const fields,
                const ZixStringView       message)
{
  (void)message;

  bool* const called = (bool*)handle;

  assert(level == SERD_LOG_LEVEL_NOTICE);
  assert(n_fields == 1);
  assert(!strcmp(fields[0].key, "TEST_KEY"));
  assert(!strcmp(fields[0].value, "TEST VALUE"));
  assert(!strcmp(message.data, "test message 42"));
  assert(message.length == strlen("test message 42"));

  *called = true;
  return SERD_SUCCESS;
}

static void
test_bad_arg(void)
{
  SerdWorld* const world  = serd_world_new(NULL);
  bool             called = false;

  serd_set_log_func(world, custom_log_func, &called);

  assert(serd_xlogf(world, SERD_LOG_LEVEL_ERROR, 0U, NULL, "%s", "") ==
         SERD_BAD_ARG);
  assert(!called);

  serd_world_free(world);
}

static void
test_default_log(void)
{
  SerdWorld* const world = serd_world_new(NULL);

  for (unsigned i = 0; i <= SERD_LOG_LEVEL_DEBUG; ++i) {
    const SerdLogLevel level = (SerdLogLevel)i;

    assert(!serd_xlogf(world, level, 0U, NULL, "test"));
  }

  serd_world_free(world);
}

static void
test_custom_log(void)
{
  SerdWorld* const world  = serd_world_new(NULL);
  bool             called = false;

  serd_set_log_func(world, custom_log_func, &called);

  const SerdLogField fields[1] = {{"TEST_KEY", "TEST VALUE"}};
  assert(!serd_xlogf(
    world, SERD_LOG_LEVEL_NOTICE, 1, fields, "test message %d", 42));

  assert(called); // Arguments are asserted by custom_log_func()
  serd_world_free(world);
}

static void
test_filename_only(void)
{
  SerdWorld* const world = serd_world_new(NULL);

  const SerdLogField fields[3] = {{"TEST_KEY", "TEST VALUE"},
                                  {"SERD_FILE", "somename"},
                                  {"SERD_CHECK", "somecheck"}};

  assert(!serd_xlogf(world, SERD_LOG_LEVEL_INFO, 3, fields, "no numbers here"));

  serd_world_free(world);
}

int
main(void)
{
  test_bad_arg();
  test_default_log();
  test_custom_log();
  test_filename_only();

  return 0;
}
