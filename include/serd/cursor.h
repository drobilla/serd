// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CURSOR_H
#define SERD_CURSOR_H

#include "serd/attributes.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

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

/// Return a view of the statement pointed to by `cursor`
SERD_API SerdStatementView
serd_cursor_get(const SerdCursor* ZIX_NULLABLE cursor);

/**
   Increment cursor to point to the next statement.

   Null is treated like an end cursor.

   @return Failure if `cursor` was already at the end.
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

/// Free `cursor`
SERD_API void
serd_cursor_free(ZixAllocator* ZIX_NULLABLE allocator,
                 SerdCursor* ZIX_NULLABLE   cursor);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CURSOR_H
