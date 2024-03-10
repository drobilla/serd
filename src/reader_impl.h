// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_IMPL_H
#define SERD_SRC_READER_IMPL_H

#include "byte_source.h"
#include "stack.h"
#include "token_header.h"

#include <serd/error.h>
#include <serd/sink.h>
#include <serd/syntax.h>
#include <serd/world.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct SerdReaderImpl {
  SerdWorld*      world;
  const SerdSink* sink;
  SerdLogFunc     error_func;
  void*           error_handle;
  TokenHeader*    rdf_first;
  TokenHeader*    rdf_rest;
  TokenHeader*    rdf_nil;
  TokenHeader*    rdf_type;
  SerdByteSource  source;
  SerdStack       stack;
  SerdSyntax      syntax;
  unsigned        next_id;
  uint8_t*        buf;
  char*           bprefix;
  size_t          bprefix_len;
  bool            strict; ///< True iff strict parsing
  bool            seen_genid;
};

#endif // SERD_SRC_READER_IMPL_H
