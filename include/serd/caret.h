// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CARET_H
#define SERD_CARET_H

#include "serd/attributes.h"
#include "serd/node.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_caret Caret
   @ingroup serd_data
   @{
*/

/// The origin of a statement in a text document
typedef struct SerdCaretImpl SerdCaret;

/**
   Create a new caret.

   Note that, to minimise model overhead, the caret does not own the name
   node, so `name` must have a longer lifetime than the caret for it to be
   valid.  That is, serd_caret_name() will return exactly the pointer
   `name`, not a copy.

   @param allocator Allocator to use for caret memory.
   @param name The name of the document or stream (usually a file URI)
   @param line The line number in the document (1-based)
   @param col The column number in the document (1-based)
   @return A new caret that must be freed with serd_caret_free()
*/
SERD_API
SerdCaret* SERD_ALLOCATED
serd_caret_new(SerdAllocator* SERD_NULLABLE allocator,
               const SerdNode* SERD_NONNULL name,
               unsigned                     line,
               unsigned                     col);

/// Return a copy of `caret`
SERD_API
SerdCaret* SERD_ALLOCATED
serd_caret_copy(SerdAllocator* SERD_NULLABLE   allocator,
                const SerdCaret* SERD_NULLABLE caret);

/// Free `caret`
SERD_API
void
serd_caret_free(SerdAllocator* SERD_NULLABLE allocator,
                SerdCaret* SERD_NULLABLE     caret);

/// Return true iff `lhs` is equal to `rhs`
SERD_PURE_API
bool
serd_caret_equals(const SerdCaret* SERD_NULLABLE lhs,
                  const SerdCaret* SERD_NULLABLE rhs);

/**
   Return the document name.

   This is typically a file URI, but may be a descriptive string node for
   statements that originate from streams.
*/
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_caret_name(const SerdCaret* SERD_NONNULL caret);

/// Return the one-relative line number in the document
SERD_PURE_API
unsigned
serd_caret_line(const SerdCaret* SERD_NONNULL caret);

/// Return the zero-relative column number in the line
SERD_PURE_API
unsigned
serd_caret_column(const SerdCaret* SERD_NONNULL caret);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CARET_H
