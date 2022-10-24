// Copyright 2018-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_CARET_H
#define SERD_SRC_CARET_H

#include "serd/node.h"

struct SerdCaretImpl {
  const SerdNode* document;
  unsigned        line;
  unsigned        col;
};

#endif // SERD_SRC_CARET_H
