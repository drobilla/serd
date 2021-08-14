// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_H
#define SERD_SRC_READER_H

#include "byte_source.h"
#include "node.h"
#include "stack.h"

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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
  SerdNode*       rdf_first;
  SerdNode*       rdf_rest;
  SerdNode*       rdf_nil;
  SerdByteSource* source;
  SerdEnv*        env;
  SerdStack       stack;
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

SerdNode*
push_node_padded(SerdReader*  reader,
                 size_t       max_length,
                 SerdNodeType type,
                 const char*  str,
                 size_t       length);

SerdNode*
push_node(SerdReader*  reader,
          SerdNodeType type,
          const char*  str,
          size_t       length);

SERD_PURE_FUNC
size_t
genid_length(const SerdReader* reader);

SERD_PURE_FUNC
bool
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

SerdStatus
read_n3_statement(SerdReader* reader);

SerdStatus
read_turtleTrigDoc(SerdReader* reader);

static inline int
peek_byte(SerdReader* reader)
{
  SerdByteSource* source = reader->source;

  return source->eof ? EOF : (int)source->read_buf[source->read_head];
}

static inline SerdStatus
skip_byte(SerdReader* reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  return serd_byte_source_advance(reader->source);
}

static inline int
eat_byte(SerdReader* reader) // FIXME
{
  const int c = peek_byte(reader);

  if (c != EOF) {
    serd_byte_source_advance(reader->source);
  }

  return c;
}

static inline int SERD_NODISCARD
eat_byte_safe(SerdReader* reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  serd_byte_source_advance(reader->source);
  return byte;
}

static inline SerdStatus SERD_NODISCARD
eat_byte_check(SerdReader* reader, const int byte)
{
  const int c = peek_byte(reader);
  if (c != byte) {
    return r_err(
      reader, SERD_ERR_BAD_SYNTAX, "expected '%c', not '%c'", byte, c);
  }

  skip_byte(reader, c);
  return SERD_SUCCESS;
}

static inline SerdStatus
eat_string(SerdReader* reader, const char* str, unsigned n)
{
  SerdStatus st = SERD_SUCCESS;

  for (unsigned i = 0; i < n; ++i) {
    if ((st = eat_byte_check(reader, str[i]))) {
      return st;
    }
  }

  return st;
}

static inline SerdStatus
push_byte(SerdReader* reader, SerdNode* node, const int c)
{
  assert(c != EOF);

  if (reader->stack.size + 1 > reader->stack.buf_size) {
    return SERD_ERR_OVERFLOW;
  }

  ((uint8_t*)reader->stack.buf)[reader->stack.size - 1] = (uint8_t)c;
  ++reader->stack.size;
  ++node->length;

  return SERD_SUCCESS;
}

static inline SerdStatus
push_bytes(SerdReader* reader, SerdNode* ref, const uint8_t* bytes, size_t len)
{
  const bool has_space = reader->stack.buf_size >= reader->stack.size + len;
  if (has_space) {
    for (unsigned i = 0; i < len; ++i) {
      push_byte(reader, ref, bytes[i]);
    }
  }

  return has_space ? SERD_SUCCESS : SERD_ERR_OVERFLOW;
}

#endif // SERD_SRC_READER_H
