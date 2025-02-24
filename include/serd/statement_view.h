// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATEMENT_VIEW_H
#define SERD_STATEMENT_VIEW_H

#include <serd/attributes.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/struct_literal.h>
#include <serd/token_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_statement_view Statement View
   @ingroup serd_streaming
   @{
*/

/// A view of a statement
typedef struct {
  SerdTokenView  subject;   ///< Subject token
  SerdTokenView  predicate; ///< Predicate token
  SerdObjectView object;    ///< Object node
  SerdTokenView  graph;     ///< Graph token
} SerdStatementView;

/// Return a view of a statement with no graph
ZIX_CONST_FUNC static inline SerdStatementView
serd_triple_view(const SerdTokenView  subject,
                 const SerdTokenView  predicate,
                 const SerdObjectView object)
{
  return SERD_STRUCT_LITERAL(SerdStatementView,
                             subject,
                             predicate,
                             object,
                             {SERD_NOTHING, ZIX_STATIC_STRING("")});
}

/// Return a view of a statement with a graph
ZIX_CONST_FUNC static inline SerdStatementView
serd_quad_view(const SerdTokenView  subject,
               const SerdTokenView  predicate,
               const SerdObjectView object,
               const SerdTokenView  graph)
{
  return SERD_STRUCT_LITERAL(
    SerdStatementView, subject, predicate, object, graph);
}

/// Return a sentinel view of an absent statement
ZIX_CONST_FUNC static inline SerdStatementView
serd_no_statement(void)
{
  return SERD_STRUCT_LITERAL(SerdStatementView,
                             serd_no_token(),
                             serd_no_token(),
                             serd_no_object(),
                             serd_no_token());
}

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATEMENT_VIEW_H
