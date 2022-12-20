// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATEMENT_H
#define SERD_STATEMENT_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/node.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stdbool.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_statement Statements
   @ingroup serd_data
   @{
*/

/// Index of a node in a statement
typedef enum {
  SERD_SUBJECT   = 0U, ///< Subject
  SERD_PREDICATE = 1U, ///< Predicate ("key")
  SERD_OBJECT    = 2U, ///< Object ("value")
  SERD_GRAPH     = 3U, ///< Graph ("context")
} SerdField;

/// Flags indicating inline abbreviation information for a statement
typedef enum {
  SERD_EMPTY_S = 1U << 0U, ///< Empty blank node subject
  SERD_EMPTY_O = 1U << 1U, ///< Empty blank node object
  SERD_ANON_S  = 1U << 2U, ///< Start of anonymous subject
  SERD_ANON_O  = 1U << 3U, ///< Start of anonymous object
  SERD_LIST_S  = 1U << 4U, ///< Start of list subject
  SERD_LIST_O  = 1U << 5U, ///< Start of list object
  SERD_TERSE_S = 1U << 6U, ///< Start of terse subject
  SERD_TERSE_O = 1U << 7U, ///< Start of terse object
} SerdStatementFlag;

/// Bitwise OR of SerdStatementFlag values
typedef uint32_t SerdStatementFlags;

/// A subject, predicate, and object, with optional graph context
typedef struct SerdStatementImpl SerdStatement;

/**
   Create a new statement.

   Note that, to minimise model overhead, statements do not own their nodes, so
   they must have a longer lifetime than the statement for it to be valid.  For
   statements in models, this is the lifetime of the model.  For user-created
   statements, the simplest way to handle this is to use `SerdNodes`.

   @param s The subject
   @param p The predicate ("key")
   @param o The object ("value")
   @param g The graph ("context")
   @param caret Optional caret at the origin of this statement
   @return A new statement that must be freed with serd_statement_free()
*/
SERD_API SerdStatement* ZIX_ALLOCATED
serd_statement_new(const SerdNode* ZIX_NONNULL   s,
                   const SerdNode* ZIX_NONNULL   p,
                   const SerdNode* ZIX_NONNULL   o,
                   const SerdNode* ZIX_NULLABLE  g,
                   const SerdCaret* ZIX_NULLABLE caret);

/// Return a copy of `statement`
SERD_API SerdStatement* ZIX_ALLOCATED
serd_statement_copy(const SerdStatement* ZIX_NULLABLE statement);

/// Free `statement`
SERD_API void
serd_statement_free(SerdStatement* ZIX_NULLABLE statement);

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

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATEMENT_H
