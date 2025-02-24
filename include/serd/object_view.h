// Copyright 2024-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_OBJECT_VIEW_H
#define SERD_OBJECT_VIEW_H

#include <serd/attributes.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/struct_literal.h>
#include <serd/token_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_object_view Object View
   @ingroup serd_data
   @{
*/

/**
   A view of an object node.

   An "object" node has a string and a type like a token, but with additional
   flags, and a second optional "meta" string used for a language tag or
   datatype URI.

   This can serve as a universal view type for any node, since the object is
   the most expresesive field in a statement.
*/
typedef struct {
  SerdNodeType  type;   ///< Node type
  ZixStringView string; ///< Node string
  SerdNodeFlags flags;  ///< Node flags
  SerdTokenView meta;   ///< View of language string or datatype URI
} SerdObjectView;

/**
   Return a view of an object node.

   This is a syntactic convenience for constructing a #SerdObjectView.
*/
ZIX_CONST_FUNC static inline SerdObjectView
serd_object_view(const SerdNodeType  type,
                 const ZixStringView string,
                 const SerdNodeFlags flags,
                 const SerdTokenView meta)
{
  return SERD_STRUCT_LITERAL(SerdObjectView, type, string, flags, meta);
}

/**
   Return a sentinel view for an absent object.

   The returned view has empty strings, and all other fields zero.
*/
ZIX_CONST_FUNC static inline SerdObjectView
serd_no_object(void)
{
  return SERD_STRUCT_LITERAL(SerdObjectView,
                             SERD_NOTHING,
                             ZIX_STATIC_STRING(""),
                             0U,
                             {SERD_NOTHING, ZIX_STATIC_STRING("")});
}

/**
   Convert an object view to a token view.

   This is a syntactic convenience for constructing a #SerdTokenView.
*/
ZIX_CONST_FUNC static inline SerdTokenView
serd_object_token_view(const SerdObjectView object)
{
  return SERD_STRUCT_LITERAL(SerdTokenView, object.type, object.string);
}

/**
   Convert a token view to an object view.

   This is a syntactic convenience for constructing a #SerdObjectView.  The
   additional fields will be unset.
*/
ZIX_CONST_FUNC static inline SerdObjectView
serd_token_object_view(const SerdTokenView token)
{
  return SERD_STRUCT_LITERAL(SerdObjectView,
                             token.type,
                             token.string,
                             0U,
                             {SERD_NOTHING, ZIX_STATIC_STRING("")});
}

/// Return whether two object views are equal
SERD_PURE_API bool
serd_object_view_equals(SerdObjectView lhs, SerdObjectView rhs);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_OBJECT_VIEW_H
