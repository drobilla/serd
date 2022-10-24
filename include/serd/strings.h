// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRINGS_H
#define SERD_STRINGS_H

#include <serd/attributes.h>
#include <serd/caret_view.h>
#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/statement_view.h>
#include <serd/token_view.h>
#include <serd/tuple.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_strings Strings
   @ingroup serd_storage
   @{
*/

/**
   A set of strings for viewing nodes.

   This owns memory that is allocated for viewing node strings.  It has a
   separate lifespan from the underlying node set, to allow for more efficient
   storage.
*/
typedef struct SerdStringsImpl SerdStrings;

/**
   Create a new set of strings for viewing nodes.

   The allocator and nodes pointers are retained, and will be used throughout
   the result's lifespan to allocate new strings.

   @param allocator Allocator for returned structure and future strings.
   @param nodes Nodes to view.
   @return A pointer to a new empty string storage.
*/
SERD_API SerdStrings* ZIX_ALLOCATED
serd_strings_new(ZixAllocator* ZIX_NULLABLE   allocator,
                 const SerdNodes* ZIX_NONNULL nodes);

/**
   Free a set of strings.

   This invalidates any views previously accessed via `strings`.
*/
SERD_API void
serd_strings_free(SerdStrings* ZIX_NULLABLE strings);

/**
   Return a view of a stored token node.

   @param strings Node strings to search.
   @param id ID of the node to view.

   @return A view of the given node, or an empty view with type #SERD_NOTHING
   if it isn't found.
*/
SERD_PURE_API SerdTokenView
serd_strings_token(SerdStrings* ZIX_NONNULL strings, SerdNodeID id);

/**
   Return a view of a stored object node.

   @param strings Node strings to search.
   @param id ID of the node to view.

   @return A view of the given node, or an empty view with type #SERD_NOTHING
   if it isn't found.  Any strings in the result point to memory owned by
   either `strings` or its underlying node set.
*/
SERD_PURE_API SerdObjectView
serd_strings_object(SerdStrings* ZIX_NONNULL strings, SerdNodeID id);

/**
   Return a view of a stored literal node.

   This is like serd_strings_object(), but the type is implicitly
   #SERD_LITERAL, and the datatype is an ID rather than a URI string.

   @param strings Node strings to search.
   @param id ID of the node to view.

   @return A view of the given node, or an empty view with type #SERD_NOTHING
   if it isn't found.  Any strings in the result point to memory owned by
   either `strings` or its underlying node set.
*/
SERD_PURE_API SerdLiteralView
serd_strings_literal(SerdStrings* ZIX_NONNULL strings, SerdNodeID id);

/**
   Return a view of a stored statement.

   @param strings Node strings to search.
   @param tuple Tuple of node IDs repesenting a statement.

   @return A view of the given nodes, possibly empty views with type
   #SERD_NOTHING if a node isn't found.
*/
SERD_PURE_API SerdStatementView
serd_strings_statement(SerdStrings* ZIX_NONNULL strings, SerdTuple tuple);

/**
   Return a caret for the document origin of a stored statement.

   @param strings Node strings to search.

   @param caret Caret for a statement in a model.  If this has a zero document
   ID, it's considered unset, and an empty (all-zero) view is returned.

   @return A view of the caret, or an empty view.
*/
SERD_PURE_API SerdCaretView
serd_strings_caret(SerdStrings* ZIX_NONNULL strings, SerdModelCaret caret);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRINGS_H
