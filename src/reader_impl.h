// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_IMPL_H
#define SERD_SRC_READER_IMPL_H

#include "byte_source.h"
#include "stack.h"

#include "serd/env.h"
#include "serd/error.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/syntax.h"
#include "serd/world.h"

#include <stdbool.h>
#include <stddef.h>

struct SerdReaderImpl {
  SerdWorld*      world;
  const SerdSink* sink;
  SerdLogFunc     error_func;
  void*           error_handle;
  SerdNode*       rdf_first;
  SerdNode*       rdf_rest;
  SerdNode*       rdf_nil;
  SerdNode*       rdf_type;
  SerdByteSource* source;
  const SerdEnv*  env;
  SerdStack       stack;
  SerdSyntax      syntax;
  SerdReaderFlags flags;
  unsigned        next_id;
  char            bprefix[24];
  size_t          bprefix_len;
  bool            strict; ///< True iff strict parsing
  bool            seen_primary_genid;
  bool            seen_secondary_genid;
};

#endif // SERD_SRC_READER_IMPL_H
