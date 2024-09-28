// Copyright 2011-2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_INTERNAL_H
#define SERD_SRC_READER_INTERNAL_H

#include "byte_source.h"
#include "read_context.h"
#include "reader_impl.h" // IWYU pragma: keep
#include "token_header.h"

#include <serd/caret_view.h>
#include <serd/event.h>
#include <serd/node_type.h>
#include <serd/reader.h>
#include <serd/status.h>
#include <serd/token_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

ZIX_LOG_FUNC(3, 4)
SerdStatus
r_err(const SerdReader* reader, SerdStatus st, const char* fmt, ...);

SerdStatus
r_err_char(const SerdReader* reader, const char* kind, int c);

SerdStatus
r_err_eof(const SerdReader* reader, SerdStatus status);

SerdStatus
r_err_expected(const SerdReader* reader, const char* expected, int c);

ZIX_NODISCARD TokenHeader*
push_node_head(SerdReader* reader, SerdNodeType type);

ZIX_NODISCARD TokenHeader*
push_node_space(SerdReader* reader, SerdNodeType type, size_t size);

ZIX_NODISCARD TokenHeader*
push_node(SerdReader* reader, SerdNodeType type, ZixStringView string);

ZIX_NODISCARD SerdStatus
push_node_termination(SerdReader* reader);

ZIX_PURE_FUNC bool
token_equals(const TokenHeader* node, ZixStringView tok);

ZIX_PURE_FUNC size_t
genid_size(const SerdReader* reader);

TokenHeader*
serd_reader_blank_id(SerdReader* reader);

void
serd_reader_set_blank_id(SerdReader*  reader,
                         TokenHeader* node,
                         size_t       buf_size);

ZIX_PURE_FUNC SerdTokenView
stack_token_view(const TokenHeader* header);

bool
pop_last_node_char(SerdReader* reader, TokenHeader* node);

SerdStatus
emit_event(const SerdReader* reader, SerdEvent event);

ZIX_NODISCARD SerdStatus
emit_statement_at(const SerdReader*  reader,
                  ReadContext        ctx,
                  const TokenHeader* object,
                  const TokenHeader* meta,
                  SerdCaretView      caret);

ZIX_NODISCARD SerdStatus
emit_statement(const SerdReader*  reader,
               ReadContext        ctx,
               const TokenHeader* object,
               const TokenHeader* meta);

ZIX_NODISCARD static inline SerdStatus
accept_failure(const SerdStatus st)
{
  return st == SERD_FAILURE ? SERD_SUCCESS : st;
}

ZIX_NODISCARD static inline SerdStatus
reject_failure(const SerdStatus st)
{
  return st == SERD_FAILURE ? SERD_BAD_SYNTAX : st;
}

ZIX_NODISCARD static inline int
peek_byte(const SerdReader* const reader)
{
  const SerdByteSource* const source = reader->source;

  return source->eof ? -1 : (int)source->read_buf[source->read_head];
}

ZIX_NODISCARD static inline SerdStatus
skip_byte(SerdReader* const reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  return accept_failure(serd_byte_source_advance(reader->source));
}

ZIX_NODISCARD static inline SerdStatus
eat_byte_check(SerdReader* const reader, const int byte)
{
  const int c = peek_byte(reader);

  SerdStatus st = accept_failure(serd_byte_source_advance(reader->source));
  if (!st && c != byte) {
    st = r_err(reader, SERD_BAD_SYNTAX, "expected '%c'", byte);
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

ZIX_NODISCARD static inline SerdStatus
eat_push_byte(SerdReader* const reader, TokenHeader* const node, const int c)
{
  const SerdStatus st = skip_byte(reader, c);

  return st ? st : push_byte(reader, node, c);
}

#endif // SERD_SRC_READER_INTERNAL_H
