// Copyright 2019-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/caret_view.h>
#include <serd/event.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stddef.h>

#define NS_EG "http://example.org/"

static const ZixStringView empty = ZIX_STATIC_STRING("");

static SerdStatus
on_event(void* const handle, const SerdEvent* const event)
{
  *(unsigned*)handle |= (1U << (unsigned)event->type);
  return SERD_SUCCESS;
}

static void
test_event(void)
{
  static const ZixStringView base_str   = ZIX_STATIC_STRING(NS_EG);
  static const SerdCaretView base_caret = {ZIX_STATIC_STRING("doc"), 1, 1};

  const SerdEvent event = serd_base_event(base_str);
  assert(zix_string_view_equals(event.caret.document, empty));
  assert(!event.caret.line);
  assert(!event.caret.column);

  const SerdSink no_sink = {NULL, NULL};
  assert(!serd_sink_event(&no_sink, serd_base_event(base_str)));

  unsigned        flags       = 0U;
  const SerdSink  sink        = {&flags, on_event};
  const SerdEvent cited_event = serd_cite_event(event, base_caret);
  assert(!serd_sink_event(&sink, cited_event));
  assert(flags == (1U << (unsigned)SERD_EVENT_BASE));
}

static void
test_bad_event(void)
{
  const SerdSink no_sink = {NULL, NULL};

  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const SerdEvent lo_type = {(SerdEventType)0, 0U, serd_no_caret(), {empty}};
  assert(serd_sink_event(&no_sink, lo_type) == SERD_BAD_ARG);

  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const SerdEvent hi_type = {(SerdEventType)5, 0U, serd_no_caret(), {empty}};
  assert(serd_sink_event(&no_sink, hi_type) == SERD_BAD_ARG);
}

int
main(void)
{
  test_event();
  test_bad_event();
  return 0;
}
