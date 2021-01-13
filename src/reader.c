// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"
#include "log.h"
#include "memory.h"
#include "namespaces.h"
#include "node_impl.h"
#include "node_internal.h"
#include "read_context.h"
#include "read_nquads.h"
#include "read_ntriples.h"
#include "read_trig.h"
#include "read_turtle.h"
#include "reader_impl.h"
#include "reader_internal.h"
#include "stack.h"
#include "string_utils.h"
#include "world_internal.h"

#include "exess/exess.h"
#include "serd/caret_view.h"
#include "serd/env.h"
#include "serd/input_stream.h"
#include "serd/log.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
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
r_err_char(SerdReader* const reader, const char* const kind, const int c)
{
  const SerdStatus st   = SERD_BAD_SYNTAX;
  const uint32_t   code = (uint32_t)c;

  return (c < 0x20 || c == 0x7F || c > 0x10FFFF)
           ? r_err(reader, st, "bad %s character", kind)
         : (c == '\'' || c >= 0x80)
           ? r_err(reader, st, "bad %s character U+%04X", kind, code)
           : r_err(reader, st, "bad %s character '%c'", kind, c);
}

SerdStatus
r_err_expected(SerdReader* const reader,
               const char* const expected,
               const int         c)
{
  const SerdStatus st   = SERD_BAD_SYNTAX;
  const uint32_t   code = (uint32_t)c;

  return (c < 0x20 || c == 0x7F || c > 0x10FFFF)
           ? r_err(reader, st, "expected %s", expected)
         : (c == '\'' || c >= 0x80)
           ? r_err(reader, st, "expected %s, not U+%04X", expected, code)
           : r_err(reader, st, "expected %s, not '%c'", expected, c);
}

SerdStatus
serd_reader_skip_until_byte(SerdReader* const reader, const uint8_t byte)
{
  SerdStatus st = SERD_SUCCESS;
  int        c  = peek_byte(reader);

  while (!st && c > 0 && c != byte) {
    st = skip_byte(reader, c);
    c  = peek_byte(reader);
  }

  return st ? st : c > 0 ? SERD_FAILURE : SERD_SUCCESS;
}

void
serd_reader_set_blank_id(SerdReader* const reader,
                         SerdNode* const   node,
                         const size_t      buf_size)
{
  const uint32_t id  = reader->next_id++;
  char* const    buf = serd_node_buffer(node);
  size_t         i   = reader->bprefix_len;

  memcpy(buf, reader->bprefix, reader->bprefix_len);
  buf[i++] = 'b';

  i += exess_write_uint(id, buf_size - i, buf + i).count;

  node->length = i;
}

size_t
genid_length(const SerdReader* const reader)
{
  return reader->bprefix_len + 10U; // + "b" + UINT32_MAX
}

SerdNode*
serd_reader_blank_id(SerdReader* const reader)
{
  SerdNode* const ref =
    push_node_padded(reader, genid_length(reader), SERD_BLANK, "", 0);

  if (ref) {
    serd_reader_set_blank_id(reader, ref, genid_length(reader) + 1U);
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
    &reader->stack, sizeof(SerdNode) + max_length + 1U, sizeof(void*));

  if (!mem) {
    return NULL;
  }

  SerdNode* const node = (SerdNode*)mem;

  node->meta   = NULL;
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

bool
token_equals(const SerdNode* const node, const char* const tok, const size_t n)
{
  if (node->length != n) {
    return false;
  }

  const char* const node_string = serd_node_string(node);
  for (size_t i = 0U; i < n; ++i) {
    if (serd_to_lower(node_string[i]) != serd_to_lower(tok[i])) {
      return false;
    }
  }

  return tok[n] == '\0';
}

SerdStatus
push_node_termination(SerdReader* const reader)
{
  const size_t top = reader->stack.size;
  const size_t pad = serd_node_pad_length(top) - top;

  return serd_stack_push(&reader->stack, pad) ? SERD_SUCCESS : SERD_BAD_STACK;
}

SerdStatus
emit_statement_at(SerdReader* const   reader,
                  const ReadContext   ctx,
                  SerdNode* const     o,
                  const SerdCaretView caret)
{
  // Push termination for the top node (object, language, or datatype)
  // (Earlier nodes have been terminated by subsequent node pushed)
  push_node_termination(reader);

  const SerdStatementView statement = {
    ctx.subject, ctx.predicate, o, ctx.graph};

  const SerdStatus st =
    serd_sink_write_statement_from(reader->sink, *ctx.flags, statement, caret);

  *ctx.flags = 0;
  return st;
}

SerdStatus
emit_statement(SerdReader* const reader,
               const ReadContext ctx,
               SerdNode* const   o)
{
  return emit_statement_at(reader, ctx, o, reader->source->caret);
}

static SerdStatus
serd_reader_prepare_if_necessary(SerdReader* const reader)
{
  return !reader->source            ? SERD_BAD_CALL
         : reader->source->prepared ? SERD_SUCCESS
                                    : serd_reader_prepare(reader);
}

static SerdStatus
read_syntax_chunk(SerdReader* const reader)
{
  return (reader->syntax == SERD_TURTLE)     ? read_turtle_chunk(reader)
         : (reader->syntax == SERD_NTRIPLES) ? read_ntriples_line(reader)
         : (reader->syntax == SERD_NQUADS)   ? read_nquads_line(reader)
         : (reader->syntax == SERD_TRIG)     ? read_trig_chunk(reader)
                                             : SERD_FAILURE;
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_SUCCESS;
  }

  SerdStatus st = serd_reader_prepare_if_necessary(reader);
  if (st) {
    return st;
  }

  if (!(reader->flags & SERD_READ_GLOBAL)) {
    const uint32_t id = serd_world_next_document_id(reader->world);

    reader->bprefix[0]  = 'f';
    reader->bprefix_len = 1U;
    reader->bprefix_len +=
      exess_write_uint(id, sizeof(reader->bprefix) - 1U, reader->bprefix + 1U)
        .count;
  }

  while (st <= SERD_FAILURE && !reader->source->eof) {
    st = read_syntax_chunk(reader);
    if (st > SERD_FAILURE && !reader->strict) {
      serd_reader_skip_until_byte(reader, '\n');
      st = SERD_SUCCESS;
    }
  }

  return accept_failure(st);
}

SerdReader*
serd_reader_new(SerdWorld* const      world,
                const SerdSyntax      syntax,
                const SerdReaderFlags flags,
                const SerdEnv* const  env,
                const SerdSink* const sink)
{
  assert(world);
  assert(env);
  assert(sink);

  ZixAllocator* const allocator  = serd_world_allocator(world);
  const SerdLimits    limits     = serd_world_limits(world);
  const size_t        stack_size = limits.reader_stack_size;
  if (stack_size < sizeof(void*) + (3U * (sizeof(SerdNode))) + 168) {
    return NULL;
  }

  SerdReader* me = (SerdReader*)zix_calloc(allocator, 1U, sizeof(SerdReader));
  if (!me) {
    return NULL;
  }

  me->world   = world;
  me->sink    = sink;
  me->env     = env;
  me->stack   = serd_stack_new(allocator, stack_size);
  me->syntax  = syntax;
  me->flags   = flags;
  me->next_id = 1;
  me->strict  = !(flags & SERD_READ_LAX);

  if (!me->stack.buf) {
    serd_wfree(world, me);
    return NULL;
  }

  // Reserve a bit of space at the end of the stack to zero pad nodes
  me->stack.buf_size -= sizeof(void*);

  me->rdf_first = push_node(me, SERD_URI, NS_RDF "first", 48);
  me->rdf_rest  = push_node(me, SERD_URI, NS_RDF "rest", 47);
  me->rdf_nil   = push_node(me, SERD_URI, NS_RDF "nil", 46);
  me->rdf_type  = push_node(me, SERD_URI, NS_RDF "type", 47);

  // The initial stack size check should cover this
  assert(me->rdf_first);
  assert(me->rdf_rest);
  assert(me->rdf_nil);
  assert(me->rdf_type);

  if (!(flags & SERD_READ_GLOBAL)) {
    me->bprefix[0]  = 'f';
    me->bprefix[1]  = '0';
    me->bprefix_len = 2U;
  }

  return me;
}

void
serd_reader_free(SerdReader* const reader)
{
  if (!reader) {
    return;
  }

  if (reader->source) {
    serd_reader_finish(reader);
  }

  serd_stack_free(serd_world_allocator(reader->world), &reader->stack);
  serd_wfree(reader->world, reader);
}

SerdStatus
serd_reader_start(SerdReader* const      reader,
                  SerdInputStream* const input,
                  const SerdNode* const  input_name,
                  const size_t           block_size)
{
  assert(reader);
  assert(input);

  if (!block_size || !input->stream) {
    return SERD_BAD_ARG;
  }

  if (reader->source) {
    return SERD_BAD_CALL;
  }

  ZixAllocator* const allocator = serd_world_allocator(reader->world);

  reader->source =
    serd_byte_source_new_input(allocator, input, input_name, block_size);

  return reader->source ? SERD_SUCCESS : SERD_BAD_ALLOC;
}

static SerdStatus
serd_reader_prepare(SerdReader* const reader)
{
  SerdStatus st = serd_byte_source_prepare(reader->source);
  if (st == SERD_SUCCESS) {
    if ((st = serd_byte_source_skip_bom(reader->source))) {
      r_err(reader, SERD_BAD_SYNTAX, "corrupt byte order mark");
    }
  } else if (st == SERD_FAILURE) {
    reader->source->eof = true;
  }
  return st;
}

SerdStatus
serd_reader_read_chunk(SerdReader* const reader)
{
  assert(reader);

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_FAILURE;
  }

  SerdStatus st = serd_reader_prepare_if_necessary(reader);
  if (!st && reader->source->eof) {
    st = serd_byte_source_advance(reader->source);
  }

  return st ? st : read_syntax_chunk(reader);
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  serd_byte_source_free(serd_world_allocator(reader->world), reader->source);
  reader->source = NULL;
  return SERD_SUCCESS;
}
