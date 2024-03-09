// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STATEMENT_H
#define SERD_SRC_STATEMENT_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/field.h"
#include "serd/node.h"
#include "serd/statement_view.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stdbool.h>

/// A subject, predicate, and object, with optional graph context
typedef struct SerdStatementImpl SerdStatement;

struct SerdStatementImpl {
  const SerdNode* ZIX_NULLABLE nodes[4];
  SerdCaret* ZIX_NULLABLE      caret;
};

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
   @param caret Optional caret at the origin of this statement
   @return A new statement that must be freed with serd_statement_free()
*/
SerdStatement* ZIX_ALLOCATED
serd_statement_new(ZixAllocator* ZIX_NULLABLE    allocator,
                   const SerdNode* ZIX_NONNULL   s,
                   const SerdNode* ZIX_NONNULL   p,
                   const SerdNode* ZIX_NONNULL   o,
                   const SerdNode* ZIX_NULLABLE  g,
                   const SerdCaret* ZIX_NULLABLE caret);

/// Return a copy of `statement`
SerdStatement* ZIX_ALLOCATED
serd_statement_copy(ZixAllocator* ZIX_NULLABLE        allocator,
                    const SerdStatement* ZIX_NULLABLE statement);

/// Free `statement`
void
serd_statement_free(ZixAllocator* ZIX_NULLABLE  allocator,
                    SerdStatement* ZIX_NULLABLE statement);

/// Return a view of `statement`
SerdStatementView
serd_statement_view(const SerdStatement* ZIX_NONNULL statement);

/// Return the given node of the statement
SERD_PURE_API const SerdNode* ZIX_NULLABLE
serd_statement_node(const SerdStatement* ZIX_NONNULL statement,
                    SerdField                        field);

/// Return the subject of the statement
SERD_PURE_API const SerdNode* ZIX_NONNULL
serd_statement_subject(const SerdStatement* ZIX_NONNULL statement);

/// Return the predicate of the statement
SERD_PURE_API const SerdNode* ZIX_NONNULL
serd_statement_predicate(const SerdStatement* ZIX_NONNULL statement);

/// Return the object of the statement
SERD_PURE_API const SerdNode* ZIX_NONNULL
serd_statement_object(const SerdStatement* ZIX_NONNULL statement);

/// Return the graph of the statement
SERD_PURE_API const SerdNode* ZIX_NULLABLE
serd_statement_graph(const SerdStatement* ZIX_NONNULL statement);

/// Return the source location where the statement originated, or NULL
SERD_PURE_API const SerdCaret* ZIX_NULLABLE
serd_statement_caret(const SerdStatement* ZIX_NONNULL statement);

/**
   Return true iff `a` is equal to `b`, ignoring statement caret metadata.

   Only returns true if nodes are equivalent, does not perform wildcard
   matching.
*/
SERD_PURE_API bool
serd_statement_equals(const SerdStatement* ZIX_NULLABLE a,
                      const SerdStatement* ZIX_NULLABLE b);

/**
   Return true iff the statement matches the given pattern.

   Nodes match if they are equivalent, or if one of them is NULL.  The
   statement matches if every node matches.
*/
SERD_PURE_API bool
serd_statement_matches(const SerdStatement* ZIX_NONNULL statement,
                       const SerdNode* ZIX_NULLABLE     subject,
                       const SerdNode* ZIX_NULLABLE     predicate,
                       const SerdNode* ZIX_NULLABLE     object,
                       const SerdNode* ZIX_NULLABLE     graph);

#endif // SERD_SRC_STATEMENT_H
