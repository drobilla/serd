// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATEMENT_H
#define SERD_STATEMENT_H

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/node.h"

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
  SERD_EMPTY_S      = 1U << 1U, ///< Empty blank node subject
  SERD_EMPTY_O      = 1U << 2U, ///< Empty blank node object
  SERD_ANON_S_BEGIN = 1U << 3U, ///< Start of anonymous subject
  SERD_ANON_O_BEGIN = 1U << 4U, ///< Start of anonymous object
  SERD_ANON_CONT    = 1U << 5U, ///< Continuation of anonymous node
  SERD_LIST_S_BEGIN = 1U << 6U, ///< Start of list subject
  SERD_LIST_O_BEGIN = 1U << 7U, ///< Start of list object
  SERD_LIST_CONT    = 1U << 8U, ///< Continuation of list
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
SERD_API SerdStatement* SERD_ALLOCATED
serd_statement_new(const SerdNode* SERD_NONNULL   s,
                   const SerdNode* SERD_NONNULL   p,
                   const SerdNode* SERD_NONNULL   o,
                   const SerdNode* SERD_NULLABLE  g,
                   const SerdCaret* SERD_NULLABLE caret);

/// Return a copy of `statement`
SERD_API SerdStatement* SERD_ALLOCATED
serd_statement_copy(const SerdStatement* SERD_NULLABLE statement);

/// Free `statement`
SERD_API void
serd_statement_free(SerdStatement* SERD_NULLABLE statement);

/// Return the given node of the statement
SERD_PURE_API const SerdNode* SERD_NULLABLE
serd_statement_node(const SerdStatement* SERD_NONNULL statement,
                    SerdField                         field);

/// Return the subject of the statement
SERD_PURE_API const SerdNode* SERD_NONNULL
serd_statement_subject(const SerdStatement* SERD_NONNULL statement);

/// Return the predicate of the statement
SERD_PURE_API const SerdNode* SERD_NONNULL
serd_statement_predicate(const SerdStatement* SERD_NONNULL statement);

/// Return the object of the statement
SERD_PURE_API const SerdNode* SERD_NONNULL
serd_statement_object(const SerdStatement* SERD_NONNULL statement);

/// Return the graph of the statement
SERD_PURE_API const SerdNode* SERD_NULLABLE
serd_statement_graph(const SerdStatement* SERD_NONNULL statement);

/// Return the source location where the statement originated, or NULL
SERD_PURE_API const SerdCaret* SERD_NULLABLE
serd_statement_caret(const SerdStatement* SERD_NONNULL statement);

/**
   Return true iff `a` is equal to `b`, ignoring statement caret metadata.

   Only returns true if nodes are equivalent, does not perform wildcard
   matching.
*/
SERD_PURE_API bool
serd_statement_equals(const SerdStatement* SERD_NULLABLE a,
                      const SerdStatement* SERD_NULLABLE b);

/**
   Return true iff the statement matches the given pattern.

   Nodes match if they are equivalent, or if one of them is NULL.  The
   statement matches if every node matches.
*/
SERD_PURE_API bool
serd_statement_matches(const SerdStatement* SERD_NONNULL statement,
                       const SerdNode* SERD_NULLABLE     subject,
                       const SerdNode* SERD_NULLABLE     predicate,
                       const SerdNode* SERD_NULLABLE     object,
                       const SerdNode* SERD_NULLABLE     graph);
/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STATEMENT_H
