// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/object_view.h>
#include <serd/statement_view.h>
#include <serd/token_view.h>

#include <stdbool.h>

bool
serd_statement_view_equals(const SerdStatementView lhs,
                           const SerdStatementView rhs)
{
  return serd_token_view_equals(lhs.subject, rhs.subject) &&
         serd_token_view_equals(lhs.predicate, rhs.predicate) &&
         serd_object_view_equals(lhs.object, rhs.object) &&
         serd_token_view_equals(lhs.graph, rhs.graph);
}
