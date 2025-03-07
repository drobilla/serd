// Copyright 2024-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_LITERAL_VIEW_H
#define SERD_LITERAL_VIEW_H

#include <serd/attributes.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <zix/string_view.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_literal_view Literal View
   @ingroup serd_data
   @{
*/

/**
   A view of a literal.

   This is a view of an internally stored liberal. It differs from
   #SerdObjectView in that it has no type (since the type is implicitly
   #SERD_LITERAL) and the meta node, if present, is stored as a node ID rather
   than a string.
*/
typedef struct {
  ZixStringView string; ///< Node string
  SerdNodeFlags flags;  ///< Node flags
  SerdNodeID    meta;   ///< ID of language tag or datatype URI
} SerdLiteralView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_LITERAL_VIEW_H
