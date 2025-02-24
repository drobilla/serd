// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOKEN_VIEW_H
#define SERD_TOKEN_VIEW_H

#include <serd/attributes.h>
#include <serd/node_type.h>
#include <serd/struct_literal.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_token_view Token View
   @ingroup serd_data
   @{
*/

/**
   A view of a token node.

   A "token" is a simple node with a type and one string, for example, a URI or
   string literal with no language tag or datatype URI.  Every field of a
   statement is a token, except for the object, which has additional fields.
*/
typedef struct {
  SerdNodeType  type;   ///< Node type
  ZixStringView string; ///< Node string
} SerdTokenView;

/**
   Return a view of a token node.

   This is a syntactic convenience for constructing a #SerdTokenView.
*/
ZIX_CONST_FUNC static inline SerdTokenView
serd_token_view(const SerdNodeType type, const ZixStringView string)
{
  return SERD_STRUCT_LITERAL(SerdTokenView, type, string);
}

/**
   Return a sentinel view for an absent token.

   The returned view has an empty string and type #SERD_NOTHING.
*/
ZIX_CONST_FUNC static inline SerdTokenView
serd_no_token(void)
{
  return SERD_STRUCT_LITERAL(
    SerdTokenView, SERD_NOTHING, ZIX_STATIC_STRING(""));
}

/// Return whether two token views are equal
SERD_PURE_API bool
serd_token_view_equals(SerdTokenView lhs, SerdTokenView rhs);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_TOKEN_VIEW_H
