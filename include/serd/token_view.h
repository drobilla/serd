// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOKEN_VIEW_H
#define SERD_TOKEN_VIEW_H

#include "serd/attributes.h"
#include "serd/node_type.h"
#include "zix/string_view.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_node_token_view Token View
   @ingroup serd_node
   @{
*/

typedef struct {
  ZixStringView string; ///< Node string
  SerdNodeType  type;   ///< Node type
} SerdTokenView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_TOKEN_VIEW_H
