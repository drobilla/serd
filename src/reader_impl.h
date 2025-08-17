// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_IMPL_H
#define SERD_SRC_READER_IMPL_H

#include "byte_source.h"
#include "stack.h"
#include "token_header.h"

#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/syntax.h>
#include <serd/world.h>

#include <stdbool.h>
#include <stddef.h>

struct SerdReaderImpl {
  SerdWorld*      world;
  const SerdSink* sink;
  TokenHeader*    rdf_first;
  TokenHeader*    rdf_rest;
  TokenHeader*    rdf_nil;
  TokenHeader*    rdf_type;
  SerdByteSource* source;
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
