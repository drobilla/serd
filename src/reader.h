// Copyright 2011-2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_H
#define SERD_SRC_READER_H

#include "byte_source.h"
#include "stack.h"
#include "token_header.h"

#include <serd/error.h>
#include <serd/event.h>
#include <serd/node_type.h>
#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  TokenHeader*    graph;
  TokenHeader*    subject;
  TokenHeader*    predicate;
  SerdEventFlags* flags;
} ReadContext;

struct SerdReaderImpl {
  SerdWorld*      world;
  const SerdSink* sink;
  SerdLogFunc     error_func;
  void*           error_handle;
  TokenHeader*    rdf_first;
  TokenHeader*    rdf_rest;
  TokenHeader*    rdf_nil;
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

SerdStatus
skip_horizontal_whitespace(SerdReader* reader);

ZIX_LOG_FUNC(3, 4)
SerdStatus
r_err(const SerdReader* reader, SerdStatus st, const char* fmt, ...);

SerdStatus
r_err_char(const SerdReader* reader, const char* kind, int c);

ZIX_NODISCARD TokenHeader*
push_node_head(SerdReader* reader, SerdNodeType type);

ZIX_NODISCARD TokenHeader*
push_node_space(SerdReader* reader, SerdNodeType type, size_t size);

ZIX_NODISCARD TokenHeader*
push_node(SerdReader* reader, SerdNodeType type, ZixStringView string);

ZIX_NODISCARD SerdStatus
push_node_termination(SerdReader* reader);

ZIX_PURE_FUNC bool
tolerate_status(const SerdReader* reader, SerdStatus status);

ZIX_PURE_FUNC size_t
genid_size(const SerdReader* reader);

TokenHeader*
blank_id(SerdReader* reader);

void
set_blank_id(SerdReader* reader, TokenHeader* node, size_t buf_size);

ZIX_PURE_FUNC SerdTokenView
stack_token_view(const TokenHeader* header);

bool
pop_last_node_char(SerdReader* reader, TokenHeader* node);

SerdStatus
emit_event(const SerdReader* reader, SerdEvent event);

SerdStatus
emit_statement(const SerdReader*  reader,
               ReadContext        ctx,
               const TokenHeader* object,
               const TokenHeader* meta);

SerdStatus
read_n3_statement(SerdReader* reader);

SerdStatus
read_turtleTrigDoc(SerdReader* reader);

static inline int
peek_byte(SerdReader* const reader)
{
  SerdByteSource* source = &reader->source;

  return source->eof ? -1 : (int)source->read_buf[source->read_head];
}

static inline SerdStatus
skip_byte(SerdReader* const reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  const SerdStatus st = serd_byte_source_advance(&reader->source);
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static inline int
eat_byte(SerdReader* const reader)
{
  const int c = peek_byte(reader);

  if (c >= 0) {
    serd_byte_source_advance(&reader->source);
  }

  return c;
}

ZIX_NODISCARD static inline int
eat_byte_safe(SerdReader* const reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  serd_byte_source_advance(&reader->source);
  return byte;
}

ZIX_NODISCARD static inline SerdStatus
eat_byte_check(SerdReader* const reader, const int byte)
{
  const int c = peek_byte(reader);
  if (c != byte) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected '%c'", byte);
  }

  skip_byte(reader, c);
  return SERD_SUCCESS;
}

ZIX_NODISCARD static inline SerdStatus
eat_string(SerdReader* const reader, const char* const str, const unsigned n)
{
  SerdStatus st = SERD_SUCCESS;

  for (unsigned i = 0; !st && i < n; ++i) {
    st = eat_byte_check(reader, str[i]);
  }

  return st;
}

ZIX_NODISCARD static inline SerdStatus
push_byte(SerdReader* const reader, TokenHeader* const node, const int c)
{
  assert(c >= 0);

  const size_t new_stack_size = reader->stack.size + 1U;
  if (new_stack_size > reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

  ((uint8_t*)reader->stack.buf)[reader->stack.size] = (uint8_t)c;
  reader->stack.size                                = new_stack_size;
  ++node->length;
  return SERD_SUCCESS;
}

ZIX_NODISCARD static inline SerdStatus
push_bytes(SerdReader* const    reader,
           TokenHeader* const   node,
           const uint8_t* const bytes,
           const unsigned       len)
{
  if (reader->stack.size + len > reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

  uint8_t* const buf = (uint8_t*)reader->stack.buf;
  memcpy(&buf[reader->stack.size], bytes, len);
  reader->stack.size += len;
  node->length += len;
  return SERD_SUCCESS;
}

#endif // SERD_SRC_READER_H
