// Copyright 2019-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/event.h>
#include <serd/handler.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <zix/bump_allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static SerdStatus
on_event(void* const handle, const SerdEvent* const event)
{
  *(unsigned*)handle |= (1U << (unsigned)event->type);
  return SERD_SUCCESS;
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  serd_failing_allocator_reset(&allocator, 0);
  assert(!serd_handler_new(&allocator.base, on_event, NULL, 0));
}

static void
destroy(void* const data)
{
  *(bool*)data = true;
}

static void
test_free(void)
{
  // Free of null should (as always) not crash
  serd_handler_free(NULL);

  // Set up a bump allocator so we can check data after (non-)free
  char             buf[512]  = {0};
  ZixBumpAllocator allocator = zix_bump_allocator(sizeof(buf), buf);

  // Set up a handler with a destroy function that sets the data to true
  SerdHandler* const handler =
    serd_handler_new(&allocator.base, on_event, destroy, sizeof(bool));

  bool* const called = (bool*)serd_handler_data(handler);

  // Free the handler and ensure the destroy function was called
  assert(!*called);
  serd_handler_free(handler);
  assert(*called);
}

static void
test_accessors(void)
{
  // Check that the data accessor returns null when there's no data
  SerdHandler* const no_data = serd_handler_new(NULL, on_event, NULL, 0);
  assert(!serd_handler_data(no_data));
  serd_handler_free(no_data);

  // Check that the data accessor returns some valid pointer when there is data
  SerdHandler* const with_data = serd_handler_new(NULL, on_event, NULL, 1);
  char* const        data      = (char*)serd_handler_data(with_data);
  assert(data);
  *data = '!';
  assert(*data == '!');

  // Check that the sink handle and function are as expected
  assert(serd_handler_sink(with_data)->handle == data);
  assert(serd_handler_sink(with_data)->on_event == on_event);

  serd_handler_free(with_data);
}

static void
test_sink(void)
{
  SerdHandler* const handler =
    serd_handler_new(NULL, on_event, NULL, sizeof(unsigned));

  const SerdSink* const sink  = serd_handler_sink(handler);
  unsigned* const       flags = (unsigned*)serd_handler_data(handler);

  *flags = 0U;

  assert(!serd_sink_event(sink, serd_base_event(zix_string("http://w3.org/"))));
  assert(*flags == (1U << (unsigned)SERD_EVENT_BASE));

  serd_handler_free(handler);
}

int
main(void)
{
  test_new_failed_alloc();
  test_free();
  test_accessors();
  test_sink();
  return 0;
}
