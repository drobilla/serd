// Copyright 2021-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/canon.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/handler.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static SerdStatus
ignore_event(void* handle, const SerdEvent* event)
{
  (void)handle;
  (void)event;
  return SERD_SUCCESS;
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdWorld* const     world     = serd_world_new(&allocator.base);
  SerdEnv* const       env = serd_env_new(&allocator.base, zix_empty_string());
  assert(world);
  assert(env);

  // Successfully allocate a canon to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  const SerdSink     target = {NULL, ignore_event};
  SerdHandler* const canon  = serd_canon_new(world, env, &target, 0U);
  assert(canon);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_canon_new(world, env, &target, 0U));
  }

  serd_handler_free(canon);
  serd_env_free(env);
  serd_world_free(world);
}

static void
test_write_failed_alloc(void)
{
#define NS_EG "http://example.org/s"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

  static const ZixStringView s_string = ZIX_STATIC_STRING(NS_EG "s");
  static const ZixStringView p_string = ZIX_STATIC_STRING(NS_EG "p");
  static const ZixStringView o_string = ZIX_STATIC_STRING("012.340");

  SerdFailingAllocator allocator = serd_failing_allocator();
  SerdWorld* const     world     = serd_world_new(&allocator.base);
  SerdEnv* const       env = serd_env_new(&allocator.base, zix_empty_string());
  assert(world);
  assert(env);

  const SerdTokenView  xsd_float = {SERD_URI, zix_string(NS_XSD "float")};
  const SerdTokenView  s         = {SERD_URI, s_string};
  const SerdTokenView  p         = {SERD_URI, p_string};
  const SerdObjectView o         = {
    SERD_LITERAL, o_string, SERD_HAS_DATATYPE, xsd_float};

  SerdSink           target = {NULL, ignore_event};
  SerdHandler* const canon  = serd_canon_new(world, env, &target, 0U);
  assert(canon);

  const SerdSink* const sink = serd_handler_sink(canon);

  // Successfully write statement to count the number of allocations
  serd_failing_allocator_reset(&allocator, SIZE_MAX);
  assert(!serd_sink_event(sink,
                          serd_statement_event(0U, serd_triple_view(s, p, o))));

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = serd_failing_allocator_reset(&allocator, 0);
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);

    const SerdStatus st = serd_sink_event(
      sink, serd_statement_event(0U, serd_triple_view(s, p, o)));

    assert(st == SERD_BAD_ALLOC);
  }

  serd_handler_free(canon);
  serd_env_free(env);
  serd_world_free(world);

#undef NS_XSD
#undef NS_EG
}

int
main(void)
{
  test_new_failed_alloc();
  test_write_failed_alloc();
  return 0;
}
