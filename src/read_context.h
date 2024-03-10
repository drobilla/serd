// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_CONTEXT_H
#define SERD_SRC_READ_CONTEXT_H

#include "serd/event.h"
#include "serd/node.h"

typedef struct {
  SerdNode*                graph;
  SerdNode*                subject;
  SerdNode*                predicate;
  SerdNode*                object;
  SerdStatementEventFlags* flags;
} ReadContext;

#endif // SERD_SRC_READ_CONTEXT_H
