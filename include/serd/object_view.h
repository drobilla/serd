// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_OBJECT_VIEW_H
#define SERD_OBJECT_VIEW_H

#include "serd/attributes.h"
#include "serd/node_flags.h"
#include "serd/node_type.h"
#include "serd/token_view.h"
#include "zix/string_view.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_object_view View
   @ingroup serd_node
   @{
*/

/**
   A view of any object node.

   This is a universal view type for any node (since the object is the most
   permissive field in a statement).

   Every node has a type, flags, and string, along with an optional "meta"
   string used for the datatype or language of literals.
*/
typedef struct {
  ZixStringView string; ///< Node string
  SerdNodeType  type;   ///< Node type
  SerdNodeFlags flags;  ///< Node flags
  SerdTokenView meta;   ///< View of language string or datatype URI
} SerdObjectView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_OBJECT_VIEW_H
