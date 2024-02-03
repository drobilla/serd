// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATEMENT_VIEW_H
#define SERD_STATEMENT_VIEW_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/object_view.h"
#include "serd/token_view.h"
#include "zix/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_statement_view Statement View
   @ingroup serd_streaming
   @{
*/

/// A view of a statement
typedef struct {
  SerdTokenView  subject;
  SerdTokenView  predicate;
  SerdObjectView object;
  SerdTokenView  graph;
} SerdStatementView;

SERD_CONST_API SerdStatementView
serd_statement_view(SerdTokenView  subject,
                    SerdTokenView  predicate,
                    SerdObjectView object,
                    SerdTokenView  graph);

SERD_CONST_API SerdStatementView
serd_statement_view_nodes(const SerdNode* ZIX_NONNULL  subject,
                          const SerdNode* ZIX_NONNULL  predicate,
                          const SerdNode* ZIX_NONNULL  object,
                          const SerdNode* ZIX_NULLABLE graph);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATEMENT_VIEW_H
