// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_H
#define SERD_SRC_READER_H

#include "byte_source.h"
#include "node.h"
#include "stack.h"
#include "try.h"

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/env.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  SerdNode*           graph;
  SerdNode*           subject;
  SerdNode*           predicate;
  SerdNode*           object;
  SerdStatementFlags* flags;
} ReadContext;

struct SerdReaderImpl {
  SerdWorld*      world;
  const SerdSink* sink;
  SerdByteSource  source;
  SerdStack       stack;
  SerdNode*       rdf_first;
  SerdNode*       rdf_rest;
  SerdNode*       rdf_nil;
  SerdNode*       rdf_type;
  SerdEnv*        env;
  SerdSyntax      syntax;
  SerdReaderFlags flags;
  unsigned        next_id;
  char            bprefix[24];
  size_t          bprefix_len;
  bool            strict; ///< True iff strict parsing
  bool            seen_genid;
};

SerdStatus
skip_horizontal_whitespace(SerdReader* reader);

SERD_LOG_FUNC(3, 4)
SerdStatus
r_err(SerdReader* reader, SerdStatus st, const char* fmt, ...);

/**
   Push the SerdNode header of a node with zero flags and length.

   If this is called, push_node_tail() must eventually be called before
   starting a new node.
*/
SerdNode*
push_node_head(SerdReader* reader, SerdNodeType type);

/**
   Push the end of a node, a null terminator and any necessary padding.

   This must be called to close the scope opened with push_node_head().
*/
SerdStatus
push_node_tail(SerdReader* reader);

/**
   Push a node with reserved space for a body.

   The body is initially all zero, as are the node's length and flags.
*/
SerdNode*
push_node_padding(SerdReader* reader, SerdNodeType type, size_t max_length);

/**
   Push a complete node with a given string body.
*/
SerdNode*
push_node(SerdReader*  reader,
          SerdNodeType type,
          const char*  str,
          size_t       length);

ZIX_PURE_FUNC int
tokcmp(const SerdNode* node, const char* tok, size_t n);

ZIX_PURE_FUNC size_t
genid_length(const SerdReader* reader);

ZIX_PURE_FUNC bool
tolerate_status(const SerdReader* reader, SerdStatus status);

SerdNode*
blank_id(SerdReader* reader);

void
set_blank_id(SerdReader* reader, SerdNode* node, size_t buf_size);

SerdStatus
emit_statement_at(SerdReader* reader,
                  ReadContext ctx,
                  SerdNode*   o,
                  SerdCaret*  caret);

SerdStatus
emit_statement(SerdReader* reader, ReadContext ctx, SerdNode* o);

static inline int
peek_byte(const SerdReader* const reader)
{
  return serd_byte_source_peek(&reader->source);
}

static inline SerdStatus
skip_byte(SerdReader* reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  return serd_byte_source_advance_past(&reader->source, byte);
}

static inline int SERD_NODISCARD
eat_byte_safe(SerdReader* reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  serd_byte_source_advance_past(&reader->source, byte);
  return byte;
}

static inline SerdStatus SERD_NODISCARD
eat_byte_check(SerdReader* reader, const int byte)
{
  const int c = peek_byte(reader);
  if (c != byte) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected '%c', not '%c'", byte, c);
  }

  skip_byte(reader, c);
  return SERD_SUCCESS;
}

static inline SerdStatus
eat_string(SerdReader* reader, const char* str, unsigned n)
{
  SerdStatus st = SERD_SUCCESS;

  for (unsigned i = 0; i < n; ++i) {
    TRY(st, eat_byte_check(reader, str[i]));
  }

  return st;
}

static inline SerdStatus
push_byte(SerdReader* reader, SerdNode* node, const int c)
{
  assert(c != EOF);

  const size_t old_size = reader->stack.size;
  if (old_size >= reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

  ++reader->stack.size;
  ++node->length;

  reader->stack.buf[old_size] = (char)c;

  return SERD_SUCCESS;
}

static inline SerdStatus
push_bytes(SerdReader* reader, SerdNode* node, const uint8_t* bytes, size_t len)
{
  if (reader->stack.buf_size < reader->stack.size + len) {
    return SERD_BAD_STACK;
  }

  memcpy(reader->stack.buf + reader->stack.size, bytes, len);
  reader->stack.size += len;
  node->length += len;
  return SERD_SUCCESS;
}

#endif // SERD_SRC_READER_H
