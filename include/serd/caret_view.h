// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CARET_VIEW_H
#define SERD_CARET_VIEW_H

#include <serd/attributes.h>
#include <serd/struct_literal.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_caret_view Caret View
   @ingroup serd_streaming
   @{
*/

/// A view of a caret, the origin of a statement in a document
typedef struct {
  ZixStringView document; ///< Name of document or input context
  size_t        line;     ///< 1-based line within document
  size_t        column;   ///< 1-based column within line
} SerdCaretView;

/**
   Return a view of an empty caret.

   Sometimes used as a sentinel, this has an empty string for a document, and
   zero for both line and column.
*/
ZIX_CONST_FUNC static inline SerdCaretView
serd_no_caret(void)
{
  return SERD_STRUCT_LITERAL(SerdCaretView, {"", 0U}, 0U, 0U);
}

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CARET_VIEW_H
