// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_MODEL_H
#define SERD_MODEL_H

#include "serd/attributes.h"
#include "serd/caret_view.h"
#include "serd/cursor.h"
#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_model Model
   @ingroup serd_storage
   @{
*/

/// An indexed set of statements
typedef struct SerdModelImpl SerdModel;

/**
   Statement ordering.

   Statements themselves always have the same fields in the same order
   (subject, predicate, object, graph), but a model can keep indices for
   different orderings to provide good performance for different kinds of
   queries.
*/
typedef enum {
  SERD_ORDER_SPO,  ///<         Subject,   Predicate, Object
  SERD_ORDER_SOP,  ///<         Subject,   Object,    Predicate
  SERD_ORDER_OPS,  ///<         Object,    Predicate, Subject
  SERD_ORDER_OSP,  ///<         Object,    Subject,   Predicate
  SERD_ORDER_PSO,  ///<         Predicate, Subject,   Object
  SERD_ORDER_POS,  ///<         Predicate, Object,    Subject
  SERD_ORDER_GSPO, ///< Graph,  Subject,   Predicate, Object
  SERD_ORDER_GSOP, ///< Graph,  Subject,   Object,    Predicate
  SERD_ORDER_GOPS, ///< Graph,  Object,    Predicate, Subject
  SERD_ORDER_GOSP, ///< Graph,  Object,    Subject,   Predicate
  SERD_ORDER_GPSO, ///< Graph,  Predicate, Subject,   Object
  SERD_ORDER_GPOS, ///< Graph,  Predicate, Object,    Subject
} SerdStatementOrder;

/// Flags that control model storage and indexing
typedef enum {
  SERD_STORE_GRAPHS = 1U << 0U, ///< Store and index the graph of statements
  SERD_STORE_CARETS = 1U << 1U, ///< Store original caret of statements
} SerdModelFlag;

/// Bitwise OR of SerdModelFlag values
typedef uint32_t SerdModelFlags;

/**
   Create a new model.

   @param world The world in which to make this model.

   @param default_order The order for the default index, which is always
   present and responsible for owning all the statements in the model.  This
   should almost always be #SERD_ORDER_SPO or #SERD_ORDER_GSPO (which
   support writing pretty documents), but advanced applications that do not want
   either of these indices can use a different order.  Additional indices can
   be added with serd_model_add_index().

   @param flags Options that control what data is stored in the model.
*/
SERD_API SerdModel* ZIX_ALLOCATED
serd_model_new(SerdWorld* ZIX_NONNULL world,
               SerdStatementOrder     default_order,
               SerdModelFlags         flags);

/// Return a deep copy of `model`
SERD_API SerdModel* ZIX_ALLOCATED
serd_model_copy(ZixAllocator* ZIX_NULLABLE   allocator,
                const SerdModel* ZIX_NONNULL model);

/// Return true iff `a` is equal to `b`, ignoring statement cursor metadata
SERD_API bool
serd_model_equals(const SerdModel* ZIX_NULLABLE a,
                  const SerdModel* ZIX_NULLABLE b);

/// Close and free `model`
SERD_API void
serd_model_free(SerdModel* ZIX_NULLABLE model);

/**
   Add an index for a particular statement order to the model.

   @return Failure if this index already exists.
*/
SERD_API SerdStatus
serd_model_add_index(SerdModel* ZIX_NONNULL model, SerdStatementOrder order);

/**
   Add an index for a particular statement order to the model.

   @return Failure if this index does not exist.
*/
SERD_API SerdStatus
serd_model_drop_index(SerdModel* ZIX_NONNULL model, SerdStatementOrder order);

/// Get the world associated with `model`
SERD_PURE_API SerdWorld* ZIX_NONNULL
serd_model_world(SerdModel* ZIX_NONNULL model);

/// Get all nodes interned in `model`
SERD_PURE_API const SerdNodes* ZIX_NONNULL
serd_model_nodes(const SerdModel* ZIX_NONNULL model);

/// Get the default statement order of `model`
SERD_PURE_API SerdStatementOrder
serd_model_default_order(const SerdModel* ZIX_NONNULL model);

/// Get the flags enabled on `model`
SERD_PURE_API SerdModelFlags
serd_model_flags(const SerdModel* ZIX_NONNULL model);

/// Return the number of statements stored in `model`
SERD_PURE_API size_t
serd_model_size(const SerdModel* ZIX_NONNULL model);

/// Return true iff there are no statements stored in `model`
SERD_PURE_API bool
serd_model_empty(const SerdModel* ZIX_NONNULL model);

/**
   Return a cursor at the start of every statement in the model.

   The returned cursor will advance over every statement in the model's default
   order.

   @param allocator The allocator used for the returned cursor.
   @param model The model that the returned cursor points to.
*/
SERD_API SerdCursor* ZIX_ALLOCATED
serd_model_begin(ZixAllocator* ZIX_NULLABLE   allocator,
                 const SerdModel* ZIX_NONNULL model);

/**
   Return a cursor past the end of the model.

   This returns the "universal" end cursor, which is equivalent to any cursor
   for this model that has reached its end.
*/
SERD_CONST_API const SerdCursor* ZIX_NONNULL
serd_model_end(const SerdModel* ZIX_NONNULL model);

/**
   Return a cursor over all statements in the model in a specific order.

   @param allocator The allocator used for the returned cursor.
   @param model The model that the returned cursor points to.
   @param order The statement order that the returned cursor advances through.
*/
SERD_API SerdCursor* ZIX_ALLOCATED
serd_model_begin_ordered(ZixAllocator* ZIX_NULLABLE   allocator,
                         const SerdModel* ZIX_NONNULL model,
                         SerdStatementOrder           order);

/**
   Search for statements that match a pattern.

   @param allocator The allocator used for the returned cursor.
   @param model The model to search in.
   @param s The subject to match, or null.
   @param p The predicate to match, or null.
   @param o The object to match, or null.
   @param g The graph to match, or null.
   @return A cursor pointing at the first match, or the end.
*/
SERD_API SerdCursor* ZIX_NULLABLE
serd_model_find(ZixAllocator* ZIX_NULLABLE   allocator,
                const SerdModel* ZIX_NONNULL model,
                const SerdNode* ZIX_NULLABLE s,
                const SerdNode* ZIX_NULLABLE p,
                const SerdNode* ZIX_NULLABLE o,
                const SerdNode* ZIX_NULLABLE g);

/**
   Search for a single node that matches a pattern.

   Exactly one of `s`, `p`, `o` must be NULL.
   This function is mainly useful for predicates that only have one value.

   @return The first matching node, or NULL if no matches are found.
*/
SERD_API const SerdNode* ZIX_NULLABLE
serd_model_get(const SerdModel* ZIX_NONNULL model,
               const SerdNode* ZIX_NULLABLE s,
               const SerdNode* ZIX_NULLABLE p,
               const SerdNode* ZIX_NULLABLE o,
               const SerdNode* ZIX_NULLABLE g);

/**
   Search for a single statement that matches a pattern.

   This function is mainly useful for predicates that only have one value.

   @return The first matching statement, or NULL if none are found.
*/
SERD_API SerdStatementView
serd_model_get_statement(const SerdModel* ZIX_NONNULL model,
                         const SerdNode* ZIX_NULLABLE s,
                         const SerdNode* ZIX_NULLABLE p,
                         const SerdNode* ZIX_NULLABLE o,
                         const SerdNode* ZIX_NULLABLE g);

/// Return true iff a statement exists
SERD_API bool
serd_model_ask(const SerdModel* ZIX_NONNULL model,
               const SerdNode* ZIX_NULLABLE s,
               const SerdNode* ZIX_NULLABLE p,
               const SerdNode* ZIX_NULLABLE o,
               const SerdNode* ZIX_NULLABLE g);

/// Return the number of matching statements
SERD_API size_t
serd_model_count(const SerdModel* ZIX_NONNULL model,
                 const SerdNode* ZIX_NULLABLE s,
                 const SerdNode* ZIX_NULLABLE p,
                 const SerdNode* ZIX_NULLABLE o,
                 const SerdNode* ZIX_NULLABLE g);

/**
   Add a statement to a model from nodes.

   This function fails if there are any active iterators on `model`.
*/
SERD_API SerdStatus
serd_model_add(SerdModel* ZIX_NONNULL       model,
               const SerdNode* ZIX_NONNULL  s,
               const SerdNode* ZIX_NONNULL  p,
               const SerdNode* ZIX_NONNULL  o,
               const SerdNode* ZIX_NULLABLE g);

/**
   Add a statement to a model from nodes with a document origin.

   This function fails if there are any active iterators on `model`.
*/
SERD_API SerdStatus
serd_model_add_from(SerdModel* ZIX_NONNULL       model,
                    const SerdNode* ZIX_NONNULL  s,
                    const SerdNode* ZIX_NONNULL  p,
                    const SerdNode* ZIX_NONNULL  o,
                    const SerdNode* ZIX_NULLABLE g,
                    SerdCaretView                caret);

/**
   Add a statement to a model.

   This function fails if there are any active iterators on `model`.
   If statement is null, then SERD_FAILURE is returned.
*/
SERD_API SerdStatus
serd_model_insert(SerdModel* ZIX_NONNULL model, SerdStatementView statement);

/**
   Add a range of statements to a model.

   This function fails if there are any active iterators on `model`.
*/
SERD_API SerdStatus
serd_model_insert_statements(SerdModel* ZIX_NONNULL  model,
                             SerdCursor* ZIX_NONNULL range);

/**
   Remove a statement from a model via an iterator.

   Calling this function invalidates all other iterators on this model.

   @param model The model which `iter` points to.

   @param cursor Cursor pointing to the element to erase.  This cursor is
   advanced to the next statement on return.
*/
SERD_API SerdStatus
serd_model_erase(SerdModel* ZIX_NONNULL model, SerdCursor* ZIX_NONNULL cursor);

/**
   Remove a range of statements from a model.

   This can be used with serd_model_find() to erase all statements in a model
   that match a pattern.

   Calling this function invalidates all iterators on `model`.

   @param model The model which `range` points to.

   @param range Range to erase, which will be empty on return.
*/
SERD_API SerdStatus
serd_model_erase_statements(SerdModel* ZIX_NONNULL  model,
                            SerdCursor* ZIX_NONNULL range);

/**
   Remove everything from a model.

   Calling this function invalidates all iterators on `model`.

   @param model The model to clear.
*/
SERD_API SerdStatus
serd_model_clear(SerdModel* ZIX_NONNULL model);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_MODEL_H
