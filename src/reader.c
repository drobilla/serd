// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"

#include "serd/log.h"

#include "byte_source.h"
#include "namespaces.h"
#include "node.h"
#include "read_nquads.h"
#include "read_ntriples.h"
#include "stack.h"
#include "statement.h"
#include "system.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SerdStatus
serd_reader_prepare(SerdReader* reader);

SerdStatus
r_err(SerdReader* const reader, const SerdStatus st, const char* const fmt, ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);

  serd_vlogf_at(
    reader->world, SERD_LOG_LEVEL_ERROR, &reader->source->caret, fmt, args);

  va_end(args);
  return st;
}

SerdStatus
skip_horizontal_whitespace(SerdReader* const reader)
{
  while (peek_byte(reader) == '\t' || peek_byte(reader) == ' ') {
    eat_byte(reader);
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_reader_skip_until_byte(SerdReader* const reader, const uint8_t byte)
{
  int c = peek_byte(reader);

  while (c != byte && c != EOF) {
    skip_byte(reader, c);
    c = peek_byte(reader);
  }

  return c == EOF ? SERD_FAILURE : SERD_SUCCESS;
}

void
set_blank_id(SerdReader* const reader,
             SerdNode* const   node,
             const size_t      buf_size)
{
  char*       buf    = (char*)(node + 1);
  const char* prefix = reader->bprefix ? (const char*)reader->bprefix : "";

  node->length =
    (size_t)snprintf(buf, buf_size, "%sb%u", prefix, reader->next_id++);
}

size_t
genid_length(const SerdReader* const reader)
{
  return reader->bprefix_len + 10; // + "b" + UINT32_MAX
}

bool
tolerate_status(const SerdReader* const reader, const SerdStatus status)
{
  if (status == SERD_SUCCESS || status == SERD_FAILURE) {
    return true;
  }

  if (status == SERD_ERR_INTERNAL || status == SERD_ERR_OVERFLOW ||
      status == SERD_ERR_BAD_WRITE || status == SERD_ERR_NO_DATA ||
      status == SERD_ERR_BAD_CALL) {
    return false;
  }

  return !reader->strict;
}

SerdNode*
blank_id(SerdReader* const reader)
{
  SerdNode* const ref =
    push_node_padded(reader, genid_length(reader), SERD_BLANK, "", 0);

  if (ref) {
    set_blank_id(reader, ref, genid_length(reader) + 1);
  }

  return ref;
}

SerdNode*
push_node_padded(SerdReader* const  reader,
                 const size_t       max_length,
                 const SerdNodeType type,
                 const char* const  str,
                 const size_t       length)
{
  // Push a null byte to ensure the previous node was null terminated
  char* terminator = (char*)serd_stack_push(&reader->stack, 1);
  if (!terminator) {
    return NULL;
  }
  *terminator = 0;

  void* mem = serd_stack_push_aligned(
    &reader->stack, sizeof(SerdNode) + max_length + 1, sizeof(SerdNode));

  if (!mem) {
    return NULL;
  }

  SerdNode* const node = (SerdNode*)mem;

  node->length = length;
  node->flags  = 0;
  node->type   = type;

  char* buf = (char*)(node + 1);
  memcpy(buf, str, length + 1);

  return node;
}

SerdNode*
push_node(SerdReader* const  reader,
          const SerdNodeType type,
          const char* const  str,
          const size_t       length)
{
  return push_node_padded(reader, length, type, str, length);
}

SerdStatus
emit_statement_at(SerdReader* const reader,
                  const ReadContext ctx,
                  SerdNode* const   o,
                  SerdCaret* const  caret)
{
  if (reader->stack.size + (2 * sizeof(SerdNode)) > reader->stack.buf_size) {
    return SERD_ERR_OVERFLOW;
  }

  /* Zero the pad of the object node on the top of the stack.  Lower nodes
     (subject and predicate) were already zeroed by subsequent pushes. */
  serd_node_zero_pad(o);

  const SerdStatement statement = {{ctx.subject, ctx.predicate, o, ctx.graph},
                                   caret};

  const SerdStatus st =
    serd_sink_write_statement(reader->sink, *ctx.flags, &statement);

  *ctx.flags = 0;
  return st;
}

SerdStatus
emit_statement(SerdReader* const reader,
               const ReadContext ctx,
               SerdNode* const   o)
{
  return emit_statement_at(reader, ctx, o, &reader->source->caret);
}

static SerdStatus
read_statement(SerdReader* const reader)
{
  return read_n3_statement(reader);
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

  if (!reader->source) {
    return SERD_ERR_BAD_CALL;
  }

  if (reader->syntax != SERD_SYNTAX_EMPTY && !reader->source->prepared) {
    SerdStatus st = serd_reader_prepare(reader);
    if (st) {
      return st;
    }
  }

  switch (reader->syntax) {
  case SERD_SYNTAX_EMPTY:
    break;
  case SERD_TURTLE:
    return read_turtleTrigDoc(reader);
  case SERD_NTRIPLES:
    return read_ntriplesDoc(reader);
  case SERD_NQUADS:
    return read_nquadsDoc(reader);
  case SERD_TRIG:
    return read_turtleTrigDoc(reader);
  }

  return SERD_SUCCESS;
}

SerdReader*
serd_reader_new(SerdWorld* const      world,
                const SerdSyntax      syntax,
                const SerdReaderFlags flags,
                SerdEnv* const        env,
                const SerdSink* const sink,
                const size_t          stack_size)
{
  assert(world);
  assert(env);
  assert(sink);

  if (stack_size < 3 * sizeof(SerdNode) + 192 + serd_node_align) {
    return NULL;
  }

  SerdReader* me = (SerdReader*)calloc(1, sizeof(SerdReader));

  me->world   = world;
  me->sink    = sink;
  me->env     = env;
  me->stack   = serd_stack_new(stack_size, serd_node_align);
  me->syntax  = syntax;
  me->flags   = flags;
  me->next_id = 1;
  me->strict  = !(flags & SERD_READ_LAX);

  // Reserve a bit of space at the end of the stack to zero pad nodes
  me->stack.buf_size -= serd_node_align;

  me->rdf_first = push_node(me, SERD_URI, NS_RDF "first", 48);
  me->rdf_rest  = push_node(me, SERD_URI, NS_RDF "rest", 47);
  me->rdf_nil   = push_node(me, SERD_URI, NS_RDF "nil", 46);

  // The initial stack size check should cover this
  assert(me->rdf_first);
  assert(me->rdf_rest);
  assert(me->rdf_nil);

  return me;
}

void
serd_reader_free(SerdReader* const reader)
{
  if (!reader) {
    return;
  }

  serd_reader_finish(reader);

  serd_free_aligned(reader->stack.buf);
  free(reader->bprefix);
  free(reader);
}

void
serd_reader_add_blank_prefix(SerdReader* const reader, const char* const prefix)
{
  assert(reader);

  free(reader->bprefix);
  reader->bprefix_len = 0;
  reader->bprefix     = NULL;

  const size_t prefix_len = prefix ? strlen(prefix) : 0;
  if (prefix_len) {
    reader->bprefix_len = prefix_len;
    reader->bprefix     = (char*)malloc(reader->bprefix_len + 1);
    memcpy(reader->bprefix, prefix, reader->bprefix_len + 1);
  }
}

static SerdStatus
skip_bom(SerdReader* const me)
{
  if (serd_byte_source_peek(me->source) == 0xEF) {
    if (serd_byte_source_advance(me->source) ||
        serd_byte_source_peek(me->source) != 0xBB ||
        serd_byte_source_advance(me->source) ||
        serd_byte_source_peek(me->source) != 0xBF ||
        serd_byte_source_advance(me->source)) {
      r_err(me, SERD_ERR_BAD_SYNTAX, "corrupt byte order mark");
      return SERD_ERR_BAD_SYNTAX;
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_reader_start(SerdReader* const reader, SerdByteSource* const byte_source)
{
  assert(reader);
  assert(byte_source);

  serd_reader_finish(reader);

  reader->source = byte_source;

  return reader->source ? SERD_SUCCESS : SERD_ERR_BAD_ARG;
}

static SerdStatus
serd_reader_prepare(SerdReader* const reader)
{
  SerdStatus st = serd_byte_source_prepare(reader->source);
  if (st == SERD_SUCCESS) {
    st = skip_bom(reader);
  } else if (st == SERD_FAILURE) {
    reader->source->eof = true;
  }
  return st;
}

SerdStatus
serd_reader_read_chunk(SerdReader* const reader)
{
  assert(reader);

  SerdStatus st = SERD_SUCCESS;
  if (!reader->source) {
    return SERD_ERR_BAD_CALL;
  }

  if (!reader->source->prepared) {
    st = serd_reader_prepare(reader);
  } else if (reader->source->eof) {
    st = serd_byte_source_advance(reader->source);
  }

  return st ? st : read_statement(reader);
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  reader->source = NULL;
  return SERD_SUCCESS;
}
