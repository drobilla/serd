// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_H
#define SERD_SRC_READER_H

#include "byte_source.h"
#include "node_impl.h"
#include "stack.h"

#include "serd/caret_view.h"
#include "serd/env.h"
#include "serd/error.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  SerdNode*                graph;
  SerdNode*                subject;
  SerdNode*                predicate;
  SerdNode*                object;
  SerdStatementEventFlags* flags;
} ReadContext;

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

ZIX_NODISCARD SerdStatus
skip_horizontal_whitespace(SerdReader* reader);

ZIX_LOG_FUNC(3, 4)
SerdStatus
r_err(SerdReader* reader, SerdStatus st, const char* fmt, ...);

SerdStatus
r_err_char(SerdReader* reader, const char* kind, int c);

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

SerdStatus
push_node_termination(SerdReader* reader);

ZIX_PURE_FUNC bool
token_equals(const SerdNode* node, const char* tok, size_t n);

ZIX_PURE_FUNC size_t
genid_length(const SerdReader* reader);

ZIX_PURE_FUNC bool
tolerate_status(const SerdReader* reader, SerdStatus status);

SerdNode*
blank_id(SerdReader* reader);

void
set_blank_id(SerdReader* reader, SerdNode* node, size_t buf_size);

ZIX_NODISCARD SerdStatus
emit_statement_at(SerdReader*   reader,
                  ReadContext   ctx,
                  SerdNode*     o,
                  SerdCaretView caret);

ZIX_NODISCARD SerdStatus
emit_statement(SerdReader* reader, ReadContext ctx, SerdNode* o);

ZIX_NODISCARD static inline SerdStatus
accept_failure(SerdStatus st)
{
  return st == SERD_FAILURE ? SERD_SUCCESS : st;
}

ZIX_NODISCARD static inline int
peek_byte(SerdReader* reader)
{
  SerdByteSource* source = reader->source;

  return source->eof ? -1 : (int)source->read_buf[source->read_head];
}

ZIX_NODISCARD static inline SerdStatus
skip_byte(SerdReader* reader, const int byte)
{
  (void)byte;

  assert(byte < 0 || peek_byte(reader) == byte);

  return accept_failure(serd_byte_source_advance(reader->source));
}

ZIX_NODISCARD static inline SerdStatus
eat_byte_check(SerdReader* reader, const int byte)
{
  SerdStatus st = SERD_BAD_SYNTAX;
  const int  c  = peek_byte(reader);
  if (c != byte) {
    return (c < 0x20 || c == '\'' || c > 0x7E)
             ? r_err(reader, st, "expected '%c'", byte)
             : r_err(reader, st, "expected '%c', not '%c'", byte, c);
  }

  return skip_byte(reader, c);
}

ZIX_NODISCARD static inline SerdStatus
eat_string(SerdReader* reader, const char* str, unsigned n)
{
  SerdStatus st = SERD_SUCCESS;

  for (unsigned i = 0; !st && i < n; ++i) {
    st = eat_byte_check(reader, str[i]);
  }

  return st;
}

ZIX_NODISCARD static inline SerdStatus
push_byte(SerdReader* reader, SerdNode* node, const int c)
{
  assert(c >= 0);

  if (reader->stack.size + 1 > reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

  ((uint8_t*)reader->stack.buf)[reader->stack.size - 1] = (uint8_t)c;
  ++reader->stack.size;
  ++node->length;
  return SERD_SUCCESS;
}

ZIX_NODISCARD static inline SerdStatus
push_bytes(SerdReader* const    reader,
           SerdNode* const      node,
           const uint8_t* const bytes,
           const size_t         len)
{
  if (reader->stack.buf_size < reader->stack.size + len) {
    return SERD_BAD_STACK;
  }

  const size_t begin = reader->stack.size - 1U;
  for (unsigned i = 0U; i < len; ++i) {
    reader->stack.buf[begin + i] = (char)bytes[i];
  }

  reader->stack.size += len;
  node->length += len;
  return SERD_SUCCESS;
}

ZIX_NODISCARD static inline SerdStatus
eat_push_byte(SerdReader* const reader, SerdNode* const node, const int c)
{
  assert(peek_byte(reader) == c);

  SerdStatus st = SERD_SUCCESS;

  if (!(st = accept_failure(serd_byte_source_advance(reader->source)))) {
    st = push_byte(reader, node, c);
  }

  return st;
}

#endif // SERD_SRC_READER_H
