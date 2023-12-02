// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"

#include "byte_source.h"
#include "log.h"
#include "memory.h"
#include "namespaces.h"
#include "node.h"
#include "read_nquads.h"
#include "read_ntriples.h"
#include "read_trig.h"
#include "read_turtle.h"
#include "stack.h"
#include "statement.h"
#include "world.h"

#include "serd/input_stream.h"
#include "serd/log.h"
#include "serd/string.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SerdStatus
serd_reader_prepare(SerdReader* reader);

SerdStatus
r_err(SerdReader* const reader, const SerdStatus st, const char* const fmt, ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);

  serd_vlogf_at(
    reader->world, SERD_LOG_LEVEL_ERROR, &reader->source.caret, fmt, args);

  va_end(args);
  return st;
}

SerdStatus
skip_horizontal_whitespace(SerdReader* const reader)
{
  int c = peek_byte(reader);
  while (c == '\t' || c == ' ') {
    skip_byte(reader, c);
    c = peek_byte(reader);
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
  char* const    buf = (char*)(node + 1);
  const unsigned id  = reader->next_id++;

  if ((reader->flags & SERD_READ_ORDERED)) {
    node->length =
      (size_t)snprintf(buf, buf_size, "%sb%09u", reader->bprefix, id);
  } else {
    node->length =
      (size_t)snprintf(buf, buf_size, "%sb%u", reader->bprefix, id);
  }
}

size_t
genid_length(const SerdReader* const reader)
{
  return reader->bprefix_len + 11;
}

bool
tolerate_status(const SerdReader* const reader, const SerdStatus status)
{
  if (status == SERD_SUCCESS || status == SERD_FAILURE) {
    return true;
  }

  if (status == SERD_BAD_STREAM || status == SERD_BAD_STACK ||
      status == SERD_BAD_WRITE || status == SERD_NO_DATA ||
      status == SERD_BAD_CALL || status == SERD_BAD_CURSOR) {
    return false;
  }

  return !reader->strict;
}

SerdNode*
blank_id(SerdReader* const reader)
{
  const size_t    length = genid_length(reader);
  SerdNode* const ref    = push_node_padding(reader, SERD_BLANK, length);

  if (ref) {
    set_blank_id(reader, ref, length + 1U);
  }

  return ref;
}

static SerdNode*
push_node_start(SerdReader* const  reader,
                const SerdNodeType type,
                const size_t       body_size)
{
  /* The top of the stack should already be aligned, because the previous node
     must be terminated before starting a new one.  This is statically
     assumed/enforced here to ensure that it's done earlier, usually right
     after writing the node body.  That way is less error-prone, because nodes
     are terminated earlier which reduces the risk of accidentally using a
     non-terminated node.  It's also faster, for two reasons:

     - Nodes, including termination, are written to the stack in a single
       sweep, as "tightly" as possible (avoiding the need to re-load that
       section of the stack into the cache for writing).

     - Pushing a new node header (this function) doesn't need to do any
       alignment calculations.
  */

  assert(!(reader->stack.size % sizeof(SerdNode)));

  const size_t    size = sizeof(SerdNode) + body_size;
  SerdNode* const node = (SerdNode*)serd_stack_push(&reader->stack, size);

  if (node) {
    node->length = 0U;
    node->flags  = 0U;
    node->type   = type;
  }

  return node;
}

/// Push a null byte to ensure the previous node was null terminated
static char*
push_node_end(SerdReader* const reader)
{
  char* const terminator = (char*)serd_stack_push(&reader->stack, 1U);

  if (terminator) {
    *terminator = 0;
  }

  return terminator;
}

SerdNode*
push_node_head(SerdReader* const reader, const SerdNodeType type)
{
  return push_node_start(reader, type, 0U);
}

SerdStatus
push_node_tail(SerdReader* const reader)
{
  if (!push_node_end(reader) ||
      !serd_stack_push_pad(&reader->stack, sizeof(SerdNode))) {
    return SERD_BAD_STACK;
  }

  assert(!(reader->stack.size % sizeof(SerdNode)));
  return SERD_SUCCESS;
}

SerdNode*
push_node_padding(SerdReader* const  reader,
                  const SerdNodeType type,
                  const size_t       max_length)
{
  SerdNode* const node = push_node_start(reader, type, max_length);
  if (!node) {
    return NULL;
  }

  memset(serd_node_buffer(node), 0, max_length);

  return !push_node_tail(reader) ? node : NULL;
}

SerdNode*
push_node(SerdReader* const  reader,
          const SerdNodeType type,
          const char* const  str,
          const size_t       length)
{
  SerdNode* const node = push_node_start(reader, type, length);
  if (!node) {
    return NULL;
  }

  node->length = length;
  memcpy(serd_node_buffer(node), str, length);

  return !push_node_tail(reader) ? node : NULL;
}

int
tokcmp(const SerdNode* const node, const char* const tok, const size_t n)
{
  return ((!node || node->length != n)
            ? -1
            : serd_strncasecmp(serd_node_string(node), tok, n));
}

SerdStatus
emit_statement_at(SerdReader* const reader,
                  const ReadContext ctx,
                  SerdNode* const   o,
                  SerdCaret* const  caret)
{
  if (reader->stack.size + (2 * sizeof(SerdNode)) > reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

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
  return emit_statement_at(reader, ctx, o, &reader->source.caret);
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

  if (!reader->source.read_buf) {
    return SERD_BAD_CALL;
  }

  if (!(reader->flags & SERD_READ_GLOBAL)) {
    reader->bprefix_len = (size_t)snprintf(reader->bprefix,
                                           sizeof(reader->bprefix),
                                           "f%u",
                                           ++reader->world->next_document_id);
  }

  if (reader->syntax != SERD_SYNTAX_EMPTY && !reader->source.prepared) {
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
                SerdEnv* const        env,
                const SerdSink* const sink)
{
  assert(world);
  assert(env);
  assert(sink);

  const size_t stack_size = world->limits.reader_stack_size;
  if (stack_size < 3 * sizeof(SerdNode) + 192 + serd_node_align) {
    return NULL;
  }

  SerdReader* me = (SerdReader*)serd_wcalloc(world, 1, sizeof(SerdReader));
  if (!me) {
    return NULL;
  }

  me->world   = world;
  me->sink    = sink;
  me->env     = env;
  me->stack   = serd_stack_new(world->allocator, stack_size, serd_node_align);
  me->syntax  = syntax;
  me->flags   = flags;
  me->next_id = 1;
  me->strict  = !(flags & SERD_READ_LAX);

  if (!me->stack.buf) {
    serd_wfree(world, me);
    return NULL;
  }

  // Reserve a bit of space at the end of the stack to zero pad nodes
  me->stack.buf_size -= serd_node_align;

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
    me->bprefix_len = 2;
  }

  return me;
}

void
serd_reader_free(SerdReader* const reader)
{
  if (!reader) {
    return;
  }

  if (reader->source.in) {
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

  if (!block_size || block_size > UINT32_MAX || !input->stream) {
    return SERD_BAD_ARG;
  }

  if (reader->source.in) {
    return SERD_BAD_CALL;
  }

  return serd_byte_source_init(
    reader->world->allocator, &reader->source, input, input_name, block_size);
}

static SerdStatus
serd_reader_prepare(SerdReader* const reader)
{
  SerdStatus st = serd_byte_source_prepare(&reader->source);
  if (st == SERD_SUCCESS) {
    if ((st = serd_byte_source_skip_bom(&reader->source))) {
      r_err(reader, SERD_BAD_SYNTAX, "corrupt byte order mark");
    }
  } else if (st == SERD_FAILURE) {
    reader->source.eof = true;
  }
  return st;
}

SerdStatus
serd_reader_read_chunk(SerdReader* const reader)
{
  assert(reader);

  const SerdStatus st = (reader->syntax == SERD_SYNTAX_EMPTY) ? SERD_FAILURE
                        : !reader->source.in                  ? SERD_BAD_CALL
                        : !reader->source.prepared ? serd_reader_prepare(reader)
                        : reader->source.eof
                          ? serd_byte_source_page(&reader->source)
                          : SERD_SUCCESS;

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

  serd_byte_source_destroy(reader->world->allocator, &reader->source);

  return SERD_SUCCESS;
}
