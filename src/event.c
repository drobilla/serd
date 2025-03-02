// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/event.h>

#include <serd/caret_view.h>
#include <serd/statement_view.h>
#include <zix/string_view.h>

SerdEvent
serd_base_event(const ZixStringView uri)
{
  SerdEvent event = {SERD_EVENT_BASE, 0U, {{"", 0}, 0, 0}, {{"", 0}}};
  event.body.uri  = uri;
  return event;
}

SerdEvent
serd_prefix_event(const ZixStringView name, const ZixStringView uri)
{
  SerdEvent event = {SERD_EVENT_PREFIX, 0U, {{"", 0}, 0, 0}, {{"", 0}}};
  event.body.prefix.prefix = name;
  event.body.prefix.suffix = uri;
  return event;
}

SerdEvent
serd_statement_event(const SerdEventFlags    flags,
                     const SerdStatementView statement)
{
  SerdEvent event = {SERD_EVENT_STATEMENT, flags, {{"", 0}, 0, 0}, {{"", 0}}};
  event.body.statement = statement;
  return event;
}

SerdEvent
serd_end_event(const ZixStringView label)
{
  SerdEvent event  = {SERD_EVENT_END, 0U, {{"", 0}, 0, 0}, {{"", 0}}};
  event.body.label = label;
  return event;
}

SerdEvent
serd_cite_event(const SerdEvent event, const SerdCaretView caret)
{
  SerdEvent cited = event;
  cited.caret     = caret;
  return cited;
}
