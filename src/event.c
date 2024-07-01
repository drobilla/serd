// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/event.h"
#include "zix/string_view.h"

SerdEvent
serd_base_event(const ZixStringView uri)
{
  SerdEvent event;
  event.base.type = SERD_BASE;
  event.base.uri  = uri;
  return event;
}

SerdEvent
serd_prefix_event(const ZixStringView name, const ZixStringView uri)
{
  SerdEvent event;
  event.prefix.type = SERD_PREFIX;
  event.prefix.name = name;
  event.prefix.uri  = uri;
  return event;
}

SerdEvent
serd_statement_event(const SerdStatementEventFlags flags,
                     const SerdStatementView       statement,
                     const SerdCaretView           caret)
{
  SerdEvent event;
  event.statement.type      = SERD_STATEMENT;
  event.statement.flags     = flags;
  event.statement.statement = statement;
  event.statement.caret     = caret;
  return event;
}

SerdEvent
serd_end_event(const ZixStringView label)
{
  SerdEvent event;
  event.end.type  = SERD_END;
  event.end.label = label;
  return event;
}
