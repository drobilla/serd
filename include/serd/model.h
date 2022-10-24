// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_MODEL_H
#define SERD_MODEL_H

#include <serd/attributes.h>
#include <serd/cursor.h>
#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/status.h>
#include <serd/tuple.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

#include <stdbool.h>
#include <stddef.h>

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
  SERD_MODEL_GRAPHS    = 1U << 0U, ///< Store and index the graph of statements
  SERD_MODEL_CARETS    = 1U << 1U, ///< Store original caret of statements
  SERD_MODEL_VARIABLES = 1U << 2U, ///< Permit storing variables
} SerdModelFlag;

/// Bitwise OR of #SerdModelFlag values
typedef unsigned SerdModelFlags;

/**
   Create a new model.

   @param world World the model will be a part of.

   @param nodes Storage for nodes in this model, which all node IDs in the
   model API are relative to.  The returned model retains this as a mutable but
   non-owning reference, that is, several models may use the same node storage.

   @param default_order Order for the default index, which is always present
   and responsible for owning all the statements in the model.  This is usually
   #SERD_ORDER_SPO or #SERD_ORDER_GSPO which matches syntax, but any order can
   be used.  Additional indices can be added with serd_model_add_index().

   @param flags Options that control what data is stored in the model.
*/
SERD_API SerdModel* ZIX_ALLOCATED
serd_model_new(SerdWorld* ZIX_NONNULL world,
               SerdNodes* ZIX_NONNULL nodes,
               SerdStatementOrder     default_order,
               SerdModelFlags         flags);

/**
   Return a deep copy of `model`.

   @param allocator Allocator for the new model.
   @param nodes Node storage for the new model.
   @param model The model to copy.
*/
SERD_API SerdModel* ZIX_ALLOCATED
serd_model_copy(ZixAllocator* ZIX_NULLABLE    allocator,
                SerdNodes* ZIX_NONNULL        nodes,
                const SerdModel* ZIX_NULLABLE model);

/// Return true iff `lhs` is equal to `rhs`, ignoring cursors
SERD_API bool
serd_model_equals(const SerdModel* ZIX_NULLABLE lhs,
                  const SerdModel* ZIX_NULLABLE rhs);

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

/// Return whether `model` has an index for a specific statement order
SERD_PURE_API bool
serd_model_has_index(const SerdModel* ZIX_NONNULL model,
                     SerdStatementOrder           order);

/// Return the world associated with `model`
SERD_PURE_API SerdWorld* ZIX_NONNULL
serd_model_world(const SerdModel* ZIX_NONNULL model);

/// Return a constant pointer to the node storage for `model`
SERD_PURE_API const SerdNodes* ZIX_NONNULL
serd_model_nodes(const SerdModel* ZIX_NONNULL model);

/// Return a mutable pointer to the node storage for `model`
SERD_PURE_API SerdNodes* ZIX_NONNULL
serd_model_mutable_nodes(SerdModel* ZIX_NONNULL model);

/// Return the default statement order of `model`
SERD_PURE_API SerdStatementOrder
serd_model_default_order(const SerdModel* ZIX_NONNULL model);

/// Return the flags enabled on `model`
SERD_PURE_API SerdModelFlags
serd_model_flags(const SerdModel* ZIX_NONNULL model);

/// Return the number of statements stored in `model`
SERD_PURE_API size_t
serd_model_size(const SerdModel* ZIX_NONNULL model);

/// Return whether there are no statements stored in `model`
SERD_PURE_API bool
serd_model_empty(const SerdModel* ZIX_NONNULL model);

/**
   Return a cursor at the first statement in the model.

   The returned cursor will advance over every statement in the model's default
   order.

   @param allocator Allocator for the returned cursor.
   @param model Model that the returned cursor points to.
*/
SERD_API SerdCursor* ZIX_ALLOCATED
serd_model_begin(ZixAllocator* ZIX_NULLABLE   allocator,
                 const SerdModel* ZIX_NONNULL model);

/**
   Return a cursor at the end of all statements in the model.

   This returns the "universal" end cursor, which is equivalent to any cursor
   for this model that has reached its end.
*/
SERD_CONST_API const SerdCursor* ZIX_NONNULL
serd_model_end(const SerdModel* ZIX_NONNULL model);

/**
   Return a cursor over all statements in the model in a specific order.

   @param allocator Allocator for the returned cursor.
   @param model Model that the returned cursor points to.
   @param order Statement order that the returned cursor advances through.
*/
SERD_API SerdCursor* ZIX_ALLOCATED
serd_model_begin_ordered(ZixAllocator* ZIX_NULLABLE   allocator,
                         const SerdModel* ZIX_NONNULL model,
                         SerdStatementOrder           order);

/**
   Search for statements that match a pattern.

   @param allocator Allocator for the returned cursor.
   @param model Model to search in.
   @param s Subject to match, or zero.
   @param p Predicate to match, or zero.
   @param o Object to match, or zero.
   @param g Graph to match, or zero.
   @return A cursor pointing at the first match, or the end.
*/
SERD_API SerdCursor* ZIX_NULLABLE
serd_model_find(ZixAllocator* ZIX_NULLABLE   allocator,
                const SerdModel* ZIX_NONNULL model,
                SerdNodeID                   s,
                SerdNodeID                   p,
                SerdNodeID                   o,
                SerdNodeID                   g);

/**
   Search for a single node that matches a pattern.

   Exactly one of `s`, `p`, `o` must be zero.  This function is mainly useful
   for predicates that only have one value.

   @return The first matching node, or zero if no matches are found.
*/
SERD_API SerdNodeID
serd_model_find_node(const SerdModel* ZIX_NONNULL model,
                     SerdNodeID                   s,
                     SerdNodeID                   p,
                     SerdNodeID                   o,
                     SerdNodeID                   g);

/// Return whether the model contains a statement matching a pattern
SERD_API bool
serd_model_ask(const SerdModel* ZIX_NONNULL model,
               SerdNodeID                   s,
               SerdNodeID                   p,
               SerdNodeID                   o,
               SerdNodeID                   g);

/// Return the number of statements matching a pattern
SERD_API size_t
serd_model_count(const SerdModel* ZIX_NONNULL model,
                 SerdNodeID                   s,
                 SerdNodeID                   p,
                 SerdNodeID                   o,
                 SerdNodeID                   g);

/**
   Insert a statement with existing nodes.

   This is the most basic statement insertion, which refers to IDs of nodes
   that are already interned.  It allocates and indexes a new statement without
   affecting the node storage.

   Invalidates all cursors on the model, except when the statement already
   existed and no errors occurred.

   @param model Model to insert into.
   @param s Subject node ID.
   @param p Predicate node ID.
   @param o Object node ID.
   @param g Graph node ID.

   @return #SERD_SUCCESS if a new statement was inserted, #SERD_FAILURE if the
   statement was already there, or an error.
*/
SERD_API SerdStatus
serd_model_insert(SerdModel* ZIX_NONNULL model,
                  SerdNodeID             s,
                  SerdNodeID             p,
                  SerdNodeID             o,
                  SerdNodeID             g);

/**
   Insert a statement of existing nodes with a document origin.

   This is like serd_model_insert(), but takes the statement nodes as a tuple,
   along with a caret for the statement's document origin.
*/
SERD_API SerdStatus
serd_model_insert_tuple(SerdModel* ZIX_NONNULL model,
                        SerdTuple              tuple,
                        SerdModelCaret         caret);

/**
   Add a range of statements to a model.

   Fails if there are any active iterators on `model`.

   @param model Model to insert statements into.

   @param cursor Cursor over the range of statements to add. This can refer to
   any model, if it refers to `model`, this call does nothing (since the
   statements are already in the model) and returns #SERD_SUCCESS.
*/
SERD_API SerdStatus
serd_model_insert_range(SerdModel* ZIX_NONNULL  model,
                        SerdCursor* ZIX_NONNULL cursor);

/**
   Remove a statement from a model via an iterator.

   Calling this function invalidates all other iterators on `model`.

   @param model Model that `cursor` points to.

   @param cursor Cursor pointing to the statement in `model` to erase, which is
   advanced to the next statement on return.

   @return #SERD_SUCCESS if the statement was erased, or #SERD_FAILURE if the
   cursor is invalid.
*/
SERD_API SerdStatus
serd_model_erase(SerdModel* ZIX_NONNULL model, SerdCursor* ZIX_NONNULL cursor);

/**
   Remove a range of statements from a model.

   This can be used with serd_model_find() to erase all statements in a model
   that match a pattern.

   Calling this function invalidates all iterators on `model`.

   @param model Model that `cursor` points to.

   @param cursor Curser over the range of statements to erase, which is
   advanced to its end on return.

   @return #SERD_SUCCESS if the range was erased, or #SERD_FAILURE if the
   cursor is invalid.
*/
SERD_API SerdStatus
serd_model_erase_range(SerdModel* ZIX_NONNULL  model,
                       SerdCursor* ZIX_NONNULL cursor);

/**
   Remove everything from a model.

   Calling this function invalidates all iterators on `model`.

   @param model Model to clear.
*/
SERD_API SerdStatus
serd_model_clear(SerdModel* ZIX_NONNULL model);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_MODEL_H
