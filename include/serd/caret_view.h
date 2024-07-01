// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CARET_VIEW_H
#define SERD_CARET_VIEW_H

#include "serd/attributes.h"
#include "serd/token_view.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_caret_view Caret View
   @ingroup serd_streaming
   @{
*/

/// A view of a caret, the origin of a statement in a document
typedef struct {
  SerdTokenView document;
  unsigned      line;
  unsigned      column;
} SerdCaretView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CARET_VIEW_H
