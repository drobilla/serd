// Copyright 2018-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_CARET_H
#define SERD_SRC_CARET_H

#include "serd/serd.h"

struct SerdCaretImpl {
  const SerdNode* file;
  unsigned        line;
  unsigned        col;
};

#endif // SERD_SRC_CARET_H
