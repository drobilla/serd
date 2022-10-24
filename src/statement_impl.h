// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STATEMENT_IMPL_H
#define SERD_SRC_STATEMENT_IMPL_H

#include <serd/node_id.h>

struct SerdStatementImpl {
  SerdNodeID nodes[4];
};

#endif // SERD_SRC_STATEMENT_IMPL_H
