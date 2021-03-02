// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SINK_H
#define SERD_SINK_H

#include "serd/serd.h"

/**
   An interface that receives a stream of RDF data.
*/
struct SerdSinkImpl {
  void*             handle;
  SerdFreeFunc      free_handle;
  SerdBaseFunc      base;
  SerdPrefixFunc    prefix;
  SerdStatementFunc statement;
  SerdEndFunc       end;
};

#endif // SERD_SINK_H
