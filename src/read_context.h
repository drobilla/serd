// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_CONTEXT_H
#define SERD_SRC_READ_CONTEXT_H

#include "token_header.h"

#include <serd/event.h>

typedef struct {
  TokenHeader*    graph;
  TokenHeader*    subject;
  TokenHeader*    predicate;
  SerdEventFlags* flags;
} ReadContext;

#endif // SERD_SRC_READ_CONTEXT_H
