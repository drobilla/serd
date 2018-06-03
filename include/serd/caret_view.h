// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_CARET_VIEW_H
#define SERD_CARET_VIEW_H

#include "serd/node.h"

SERD_BEGIN_DECLS

/**
   @ingroup serd_streaming
   @{
*/

typedef struct {
  const SerdNode* SERD_NULLABLE document;
  unsigned                      line;
  unsigned                      column;
} SerdCaretView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_CARET_VIEW_H
