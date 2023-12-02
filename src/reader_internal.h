// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READER_INTERNAL_H
#define SERD_SRC_READER_INTERNAL_H

#include "byte_source.h"
#include "node_impl.h"
#include "read_context.h"
#include "reader_impl.h"

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/status.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

SERD_LOG_FUNC(3, 4)
SerdStatus
r_err(SerdReader* reader, SerdStatus st, const char* fmt, ...);

SerdStatus
r_err_char(SerdReader* reader, const char* kind, int c);

SerdStatus
r_err_expected(SerdReader* reader, const char* expected, int c);

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

ZIX_PURE_FUNC bool
token_equals(const SerdNode* node, const char* tok, size_t n);

ZIX_PURE_FUNC size_t
genid_length(const SerdReader* reader);

SerdNode*
serd_reader_blank_id(SerdReader* reader);

/// Write a blank label to node, which must have genid_length() + 1 bytes
void
serd_reader_set_blank_id(SerdReader* reader, SerdNode* node);

SERD_NODISCARD SerdStatus
emit_statement_at(SerdReader* reader,
                  ReadContext ctx,
                  SerdNode*   o,
                  SerdCaret*  caret);

SERD_NODISCARD SerdStatus
emit_statement(SerdReader* reader, ReadContext ctx, SerdNode* o);

SERD_NODISCARD static inline SerdStatus
accept_failure(const SerdStatus st)
{
  return st == SERD_FAILURE ? SERD_SUCCESS : st;
}

SERD_NODISCARD static inline SerdStatus
reject_failure(const SerdStatus st)
{
  return st == SERD_FAILURE ? SERD_BAD_SYNTAX : st;
}

SERD_NODISCARD static inline int
peek_byte(const SerdReader* const reader)
{
  return serd_byte_source_peek(&reader->source);
}

SERD_NODISCARD static inline SerdStatus
skip_byte(SerdReader* const reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  return accept_failure(serd_byte_source_advance_past(&reader->source, byte));
}

SERD_NODISCARD static inline int
eat_byte_safe(SerdReader* reader, const int byte)
{
  (void)byte;

  assert(peek_byte(reader) == byte);

  serd_byte_source_advance_past(&reader->source, byte);
  return byte;
}

SERD_NODISCARD static inline SerdStatus
eat_byte_check(SerdReader* const reader, const int byte)
{
  const int        c = peek_byte(reader);
  const SerdStatus st =
    accept_failure(serd_byte_source_advance(reader->source));

  return (st || c == byte) ? st
         : (c < 0x20 || c == '\'' || c > 0x7E)
           ? r_err(reader, SERD_BAD_SYNTAX, "expected '%c'", byte)
           : r_err(reader, SERD_BAD_SYNTAX, "expected '%c', not '%c'", byte, c);
}

SERD_NODISCARD static inline SerdStatus
push_byte(SerdReader* reader, SerdNode* node, const int c)
{
  assert(c >= 0);

  const size_t old_size = reader->stack.size;
  if (old_size >= reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

  ++reader->stack.size;
  ++node->length;

  reader->stack.buf[old_size] = (char)c;

  return SERD_SUCCESS;
}

SERD_NODISCARD static inline SerdStatus
push_bytes(SerdReader* const    reader,
           SerdNode* const      node,
           const uint8_t* const bytes,
           const size_t         len)
{
  if (reader->stack.buf_size < reader->stack.size + len) {
    return SERD_BAD_STACK;
  }

  memcpy(reader->stack.buf + reader->stack.size, bytes, len);
  reader->stack.size += len;
  node->length += len;
  return SERD_SUCCESS;
}

SERD_NODISCARD static inline SerdStatus
eat_push_byte(SerdReader* const reader, SerdNode* const node, const int c)
{
  const SerdStatus st = skip_byte(reader, c);

  SerdStatus st = SERD_SUCCESS;

  if (!(st =
          accept_failure(serd_byte_source_advance_past(&reader->source, c)))) {
    st = push_byte(reader, node, c);
  }

  return st;
}

#endif // SERD_SRC_READER_INTERNAL_H
