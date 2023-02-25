// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"

#include "byte_source.h"
#include "memory.h"
#include "namespaces.h"
#include "node.h"
#include "node_impl.h"
#include "read_nquads.h"
#include "read_ntriples.h"
#include "read_trig.h"
#include "read_turtle.h"
#include "stack.h"
#include "string_utils.h"
#include "world.h"

#include "exess/exess.h"
#include "serd/input_stream.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

static SerdStatus
serd_reader_prepare(SerdReader* reader);

SerdStatus
r_err(SerdReader* const reader, const SerdStatus st, const char* const fmt, ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);
  const SerdError e = {st, &reader->source->caret, fmt, &args};
  serd_world_error(reader->world, &e);
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

bool
tolerate_status(const SerdReader* const reader, const SerdStatus status)
{
  if (status == SERD_SUCCESS || status == SERD_FAILURE) {
    return true;
  }

  if (status == SERD_BAD_STREAM || status == SERD_BAD_STACK ||
      status == SERD_BAD_WRITE || status == SERD_NO_DATA ||
      status == SERD_BAD_CALL) {
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
    set_blank_id(reader, ref, genid_length(reader) + 1U);
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
    if (serd_to_upper(node_string[i]) != serd_to_upper(tok[i])) {
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
emit_statement(SerdReader* const reader,
               const ReadContext ctx,
               SerdNode* const   o)
{
  // Push termination for the top node (object, language, or datatype)
  // (Earlier nodes have been terminated by subsequent node pushed)
  push_node_termination(reader);

  const SerdStatus st = serd_sink_write(
    reader->sink, *ctx.flags, ctx.subject, ctx.predicate, o, ctx.graph);

  *ctx.flags = 0;
  return st;
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

  if (!reader->source) {
    return SERD_BAD_CALL;
  }

  if (!(reader->flags & SERD_READ_GLOBAL)) {
    const uint32_t id = ++reader->world->next_document_id;

    reader->bprefix[0]  = 'f';
    reader->bprefix_len = 1U;
    reader->bprefix_len +=
      exess_write_uint(id, sizeof(reader->bprefix) - 1U, reader->bprefix + 1U)
        .count;
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
    return read_turtleDoc(reader);
  case SERD_NTRIPLES:
    return read_ntriplesDoc(reader);
  case SERD_NQUADS:
    return read_nquadsDoc(reader);
  case SERD_TRIG:
    return read_trigDoc(reader);
  }

  return SERD_SUCCESS;
}

SerdReader*
serd_reader_new(SerdWorld* const      world,
                const SerdSyntax      syntax,
                const SerdReaderFlags flags,
                const SerdSink* const sink)
{
  assert(world);
  assert(sink);

  const size_t stack_size = world->limits.reader_stack_size;
  if (stack_size < sizeof(void*) + (3U * (sizeof(SerdNode))) + 168) {
    return NULL;
  }

  SerdReader* me = (SerdReader*)serd_wcalloc(world, 1, sizeof(SerdReader));
  if (!me) {
    return NULL;
  }

  me->world   = world;
  me->sink    = sink;
  me->stack   = serd_stack_new(world->allocator, stack_size);
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

  // The initial stack size check should cover this
  assert(me->rdf_first);
  assert(me->rdf_rest);
  assert(me->rdf_nil);

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

  serd_stack_free(reader->world->allocator, &reader->stack);
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

  reader->source = serd_byte_source_new_input(
    reader->world->allocator, input, input_name, block_size);

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

  SerdStatus st = SERD_SUCCESS;
  if (reader->syntax != SERD_SYNTAX_EMPTY) {
    if (!reader->source) {
      return SERD_BAD_CALL;
    }

    if (!reader->source->prepared) {
      st = serd_reader_prepare(reader);
    } else if (reader->source->eof) {
      st = serd_byte_source_advance(reader->source);
    }
  }

  if (st) {
    return st;
  }

  switch (reader->syntax) {
  case SERD_SYNTAX_EMPTY:
    break;
  case SERD_TURTLE:
    return read_turtle_statement(reader);
  case SERD_NTRIPLES:
    return read_ntriples_line(reader);
  case SERD_NQUADS:
    return read_nquads_line(reader);
  case SERD_TRIG:
    return read_trig_statement(reader);
  }

  return SERD_FAILURE;
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  serd_byte_source_free(reader->world->allocator, reader->source);
  reader->source = NULL;
  return SERD_SUCCESS;
}
