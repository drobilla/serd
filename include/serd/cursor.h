// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CURSOR_H
#define SERD_CURSOR_H

#include "serd/attributes.h"
#include "serd/statement.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_cursor Cursor
   @ingroup serd
   @{
*/

/**
   A cursor that iterates over statements in a model.

   A cursor is a smart iterator that visits all statements that match a
   pattern.
*/
typedef struct SerdCursorImpl SerdCursor;

/// Return a new copy of `cursor`
SERD_API
SerdCursor* SERD_ALLOCATED
serd_cursor_copy(const SerdCursor* SERD_NULLABLE cursor);

/// Return the statement pointed to by `cursor`
SERD_API
const SerdStatement* SERD_NULLABLE
serd_cursor_get(const SerdCursor* SERD_NONNULL cursor);

/**
   Increment cursor to point to the next statement.

   @return Failure if `cursor` was already at the end.
*/
SERD_API
SerdStatus
serd_cursor_advance(SerdCursor* SERD_NONNULL cursor);

/// Return true if the cursor has reached its end
SERD_PURE_API
bool
serd_cursor_is_end(const SerdCursor* SERD_NULLABLE cursor);

/**
   Return true iff `lhs` equals `rhs`.

   Two cursors are equivalent if they point to the same statement in the same
   index in the same model, or are both the end of the same model.  Note that
   two cursors can point to the same statement but not be equivalent, since
   they may have reached the statement via different indices.
*/
SERD_PURE_API
bool
serd_cursor_equals(const SerdCursor* SERD_NULLABLE lhs,
                   const SerdCursor* SERD_NULLABLE rhs);

/// Free `cursor`
SERD_API
void
serd_cursor_free(SerdCursor* SERD_NULLABLE cursor);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CURSOR_H
