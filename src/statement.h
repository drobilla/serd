// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STATEMENT_H
#define SERD_SRC_STATEMENT_H

#include "statement_impl.h"

#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/statement_view.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

/// A subject, predicate, and object, with optional graph context
typedef struct SerdStatementImpl SerdStatement;

/**
   Create a new statement.

   Note that, to minimise model overhead, statements do not own their nodes, so
   they must have a longer lifetime than the statement for it to be valid.  For
   statements in models, this is the lifetime of the model.  For user-created
   statements, the simplest way to handle this is to use `SerdNodes`.

   @param allocator Allocator for the returned statement.
   @param s The subject
   @param p The predicate ("key")
   @param o The object ("value")
   @param g The graph ("context")
   @return A new statement that must be freed with serd_statement_free()
*/
SerdStatement* ZIX_ALLOCATED
serd_statement_new(ZixAllocator* ZIX_NULLABLE allocator,
                   SerdNodeID                 s,
                   SerdNodeID                 p,
                   SerdNodeID                 o,
                   SerdNodeID                 g);

/// Free `statement`
void
serd_statement_free(ZixAllocator* ZIX_NULLABLE  allocator,
                    SerdStatement* ZIX_NULLABLE statement);

/// Return a view of `statement`
ZIX_PURE_FUNC SerdStatementView
serd_statement_statement_view(const SerdStatement* ZIX_NONNULL statement,
                              const SerdNodes* ZIX_NONNULL     nodes);

#endif // SERD_SRC_STATEMENT_H
