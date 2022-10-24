// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CURSOR_H
#define SERD_CURSOR_H

#include <serd/attributes.h>
#include <serd/field.h>
#include <serd/model_caret.h>
#include <serd/nodes.h>
#include <serd/status.h>
#include <serd/tuple.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_cursor Cursor
   @ingroup serd_storage
   @{
*/

/**
   A cursor that iterates over statements in a model.

   A cursor is a smart iterator that visits all statements that match a
   pattern.
*/
typedef struct SerdCursorImpl SerdCursor;

/// Return a new copy of `cursor`
SERD_API SerdCursor* ZIX_ALLOCATED
serd_cursor_copy(ZixAllocator* ZIX_NULLABLE     allocator,
                 const SerdCursor* ZIX_NULLABLE cursor);

/// Free `cursor`
SERD_API void
serd_cursor_free(ZixAllocator* ZIX_NULLABLE allocator,
                 SerdCursor* ZIX_NULLABLE   cursor);

/// Return the ID of a node in the statement pointed to by `cursor`
SERD_API SerdNodeID
serd_cursor_field(const SerdCursor* ZIX_NULLABLE cursor, SerdField field);

/// Return the IDs of the nodes in the statement pointed to by `cursor`
SERD_API SerdTuple
serd_cursor_tuple(const SerdCursor* ZIX_NULLABLE cursor);

/// Return the origin of the statement pointed to by `cursor`, if any
SERD_API SerdModelCaret
serd_cursor_caret(const SerdCursor* ZIX_NULLABLE cursor);

/**
   Increment cursor to point to the next statement.

   Null is treated like an end cursor.

   @return #SERD_SUCCESS, #SERD_FAILURE if the cursor was incremented to the
   end, or #SERD_BAD_CURSOR if it was already there.
*/
SERD_API SerdStatus
serd_cursor_advance(SerdCursor* ZIX_NULLABLE cursor);

/**
   Return true if the cursor has reached its end.

   Null is treated like an end cursor.
*/
SERD_PURE_API bool
serd_cursor_is_end(const SerdCursor* ZIX_NULLABLE cursor);

/**
   Return true iff `lhs` equals `rhs`.

   Two cursors are equivalent if they point to the same statement in the same
   index in the same model, or are both the end of the same model.  Note that
   two cursors can point to the same statement but not be equivalent, since
   they may have reached the statement via different indices.

   Null is treated like an end cursor.
*/
SERD_PURE_API bool
serd_cursor_equals(const SerdCursor* ZIX_NULLABLE lhs,
                   const SerdCursor* ZIX_NULLABLE rhs);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CURSOR_H
